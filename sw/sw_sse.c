#include "sse.h"

extern double sse_matrix_t, sse_tracking_t;
extern double sse1_matrix_t, sse1_tracking_t;


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

void sw_sse(char **q, char **r, float gap_open, float gap_extend, 
	    float *score, char **m, char **n, int *start1, int *start2) {
  /*  
    if (H == NULL) {
      H = (float *) _mm_malloc(4 * 100 * 800 * sizeof(float), 16);
      C = (int *) _mm_malloc(4 * 100 * 800 * sizeof(int), 16);
    }
  */
  float match = 5.0f, mismatch = -4.0f;

  int len, q_lens[4], r_lens[4], alig_lens[4];
  int max_q_len = 0;
  int max_r_len = 0;
  for (int i = 0; i < 4; i++) {
    len = strlen(q[i]);
    if (len > max_q_len) max_q_len = len;
    q_lens[i] = len;

    len = strlen(r[i]);
    if (len > max_r_len) max_r_len = len;
    r_lens[i] = len;

    //printf("length of q[%i] = %i\n", i, q_lens[i]);
    //printf("length of r[%i] = %i\n", i, r_lens[i]);
  }
  //  printf("max. length of query = %i, ref. = %i\n", max_q_len, max_r_len);

  float profile[128][128];
  profile['A']['A'] = 5.0f; profile['C']['A'] = -4.0f; profile['T']['A'] = -4.0f; profile['G']['A'] = -4.0f;
  profile['A']['C'] = -4.0f; profile['C']['C'] = 5.0f; profile['T']['C'] = -4.0f; profile['G']['C'] = -4.0f;
  profile['A']['G'] = -4.0f; profile['C']['T'] = -4.0f; profile['T']['T'] = 5.0f; profile['G']['T'] = -4.0f;
  profile['A']['T'] = -4.0f; profile['C']['G'] = -4.0f; profile['T']['G'] = -4.0f; profile['G']['G'] = 5.0f;

  int matrix_size = 4 * max_q_len * max_r_len * sizeof(float);

  float *H = (float *) _mm_malloc(matrix_size, 16);
  float *D = (float *) _mm_malloc(matrix_size, 16);
  float *E = (float *) _mm_malloc(matrix_size, 16);
  float *F = (float *) _mm_malloc(matrix_size, 16);

  double partial_t;
  partial_t = tic();
  sse_matrix(4, q, q_lens, max_q_len, r, r_lens, max_r_len, 
	     profile, gap_open, gap_extend, H, D, E, F, score);
  sse_matrix_t += toc(partial_t);

  /*
  char filename[200];
  for (int i = 0; i < 4; i++) {
    sprintf(filename, "/tmp/sse-%i.score", i);
    save_float_matrix(H, max_q_len, max_r_len,
		      q[i], q_lens[i], r[i], r_lens[i], 
		      i, 4, filename);
  }
  */

  partial_t = tic();
  simd_traceback(4, 4, q, q_lens, max_q_len, r, r_lens, max_r_len,
		 gap_open, gap_extend,
		 H, D, E, F, score,
		 m, start1,
		 n, start2, 
		 alig_lens);
  sse_tracking_t += toc(partial_t);
  
  _mm_free(H);
  _mm_free(D);
  _mm_free(E);
  _mm_free(F);
  //  _mm_free(M);
}

//-------------------------------------------------------------------------

void sse_matrix(int num_seqs, 
		char **q, int *q_len, int max_q_len,
		char **r, int *r_len, int max_r_len,
		float profile[128][128], float gap_open, float gap_extend,
		float *H, float *D, float *E, float *F, float *max_score) {
  
  const int depth = 4;

  __m128 h_simd, e_simd, f_simd, diagonal_simd;
  __m128 temp_simd, subst_simd;

  __m128 score_simd = _mm_setzero_ps();
  __m128 zero_simd = _mm_setzero_ps();
  __m128 gap_open_simd = _mm_set1_ps(gap_open);
  __m128 gap_extend_simd = _mm_set1_ps(gap_extend);

  int offset, index, idx, j_depth;
  int q_len_depth = depth * max_q_len;

  h_simd = zero_simd;
  e_simd = zero_simd;
  
  for (int j = 0; j < max_q_len; j++) {

    j_depth = depth * j;
    
    // left value: gap in reference
    e_simd = _mm_max_ps(_mm_sub_ps(e_simd, gap_extend_simd), 
			_mm_sub_ps(h_simd, gap_open_simd));
    
    // diagonal value: match or mismatch
    subst_simd = _mm_set_ps(profile[q[3][j]][r[3][0]], 
			    profile[q[2][j]][r[2][0]], 
			    profile[q[1][j]][r[1][0]], 
			    profile[q[0][j]][r[0][0]]);

    diagonal_simd = _mm_add_ps(zero_simd, subst_simd);
    
    // up value: gap in query
    f_simd = _mm_max_ps(_mm_sub_ps(zero_simd, gap_extend_simd), 
			_mm_sub_ps(zero_simd, gap_open_simd));
        
    // get max. value
    h_simd = _mm_max_ps(_mm_max_ps(diagonal_simd, 
				   _mm_max_ps(e_simd, f_simd)), 
			zero_simd);
    score_simd = _mm_max_ps(score_simd, h_simd);

    // update matrices
    _mm_store_ps(&H[j_depth], h_simd);
    _mm_store_ps(&E[j_depth], e_simd);
    _mm_store_ps(&F[j_depth], f_simd); 
    _mm_store_ps(&D[j_depth], diagonal_simd);

  }

  int target = 0;
  for (int i = 1; i < max_r_len; i++) {
    
    h_simd = zero_simd;
    e_simd = zero_simd;
    temp_simd = zero_simd;

    idx = i * q_len_depth;

    for (int j = 0; j < max_q_len; j++) {
      j_depth = depth * j;
      offset = idx + j_depth;

      // left value: gap in reference
      e_simd = _mm_max_ps(_mm_sub_ps(e_simd, gap_extend_simd), 
			  _mm_sub_ps(h_simd, gap_open_simd));

      // diagonal value: match or mismatch
      diagonal_simd = _mm_add_ps(temp_simd,
				 _mm_set_ps((q_len[3] > j && r_len[3] > i) ? profile[q[3][j]][r[3][i]] : -1000.0f, 
					    (q_len[2] > j && r_len[2] > i) ? profile[q[2][j]][r[2][i]] : -1000.0f, 
					    (q_len[1] > j && r_len[1] > i) ? profile[q[1][j]][r[1][i]] : -1000.0f, 
					    (q_len[0] > j && r_len[0] > i) ? profile[q[0][j]][r[0][i]] : -1000.0f)
				 );

      // up value: gap in query
      temp_simd = _mm_load_ps(&H[offset - q_len_depth]);

      f_simd = _mm_load_ps(&F[offset - q_len_depth]);
      f_simd = _mm_max_ps(_mm_sub_ps(f_simd,gap_extend_simd), 
      			      _mm_sub_ps(temp_simd, gap_open_simd));

      // get max. value
      h_simd = _mm_max_ps(_mm_max_ps(diagonal_simd, 
				     _mm_max_ps(e_simd, f_simd)), 
			  zero_simd);
      score_simd = _mm_max_ps(score_simd, h_simd);

      // update matrices
      _mm_store_ps(&H[offset], h_simd);
      _mm_store_ps(&E[offset], e_simd);
      _mm_store_ps(&F[offset], f_simd); 
      _mm_store_ps(&D[offset], diagonal_simd);
    }
  }
  _mm_store_ps(max_score, score_simd);
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

void sw_sse1(char **q, char **r, float gap_open, float gap_extend, 
	     float *score, char **m, char **n, int *start1, int *start2) {

  float match = 5.0f, mismatch = -4.0f;
  
  int len, q_lens[4], r_lens[4], alig_lens[4];
  int max_q_len = 0;
  int max_r_len = 0;
  for (int i = 0; i < 4; i++) {
    len = strlen(q[i]);
    if (len > max_q_len) max_q_len = len;
    q_lens[i] = len;
    
    len = strlen(r[i]);
    if (len > max_r_len) max_r_len = len;
    r_lens[i] = len;
  }
  //  printf("max. length of query = %i, ref. = %i\n", max_q_len, max_r_len);
  
  float profile[128][128];
  profile['A']['A'] = 5.0f; profile['C']['A'] = -4.0f; profile['T']['A'] = -4.0f; profile['G']['A'] = -4.0f;
  profile['A']['C'] = -4.0f; profile['C']['C'] = 5.0f; profile['T']['C'] = -4.0f; profile['G']['C'] = -4.0f;
  profile['A']['G'] = -4.0f; profile['C']['T'] = -4.0f; profile['T']['T'] = 5.0f; profile['G']['T'] = -4.0f;
  profile['A']['T'] = -4.0f; profile['C']['G'] = -4.0f; profile['T']['G'] = -4.0f; profile['G']['G'] = 5.0f;
  
  int matrix_size = 4 * max_q_len * max_r_len * sizeof(float);
  
  float *H = (float *) _mm_malloc(matrix_size, 16);
  float *F = (float *) _mm_malloc(4 * max_q_len * sizeof(float), 16);
  int *C = (int *) _mm_malloc(matrix_size, 16);

  double partial_t;
  partial_t = tic();
  sse1_matrix(4, q, q_lens, max_q_len, r, r_lens, max_r_len, 
	      profile, gap_open, gap_extend, H, F, C, score);
  sse1_matrix_t += toc(partial_t);

  partial_t = tic();
  /*
  simd_traceback1(4, 4, q, q_lens, max_q_len, r, r_lens, max_r_len,
		  gap_open, gap_extend,
		  H, C, score,
		  m, start1,
		  n, start2, 
		  alig_lens);
  */
  sse1_tracking_t += toc(partial_t);
  
  _mm_free(H);
  _mm_free(F);
  _mm_free(C);
}

//-------------------------------------------------------------------------

void sse1_matrix(int num_seqs, 
		 char **q, int *q_len, int max_q_len,
		 char **r, int *r_len, int max_r_len,
		 float profile[128][128], float gap_open, float gap_extend,
		 float *H, float *F, int *C, float *max_score) {
  
  const int depth = 4;

  __m128 h_simd, e_simd, f_simd, diagonal_simd;
  __m128 temp_simd, subst_simd;

  __m128i zeroi   = _mm_set_epi32(0, 0, 0, 0);

  __m128 score_simd = _mm_setzero_ps();
  __m128 zero_simd = _mm_setzero_ps();
  __m128 one_simd = _mm_set1_ps(1);
  __m128 gap_open_simd = _mm_set1_ps(gap_open);
  __m128 gap_extend_simd = _mm_set1_ps(gap_extend);

  __m128 max_de, max_fz;
  __m128 cmp_de, cmp_fz, cmp_de_fz;
  __m128i c;

  int offset, index, idx, j_depth;
  int q_len_depth = depth * max_q_len;

  h_simd = zero_simd;
  e_simd = zero_simd;
  
  for (int j = 0; j < max_q_len; j++) {

    j_depth = depth * j;
    
    // left value: gap in reference
    e_simd = _mm_max_ps(_mm_sub_ps(e_simd, gap_extend_simd), 
			_mm_sub_ps(h_simd, gap_open_simd));

    // diagonal value: match or mismatch
    subst_simd = _mm_set_ps(profile[q[3][j]][r[3][0]], 
			    profile[q[2][j]][r[2][0]], 
			    profile[q[1][j]][r[1][0]], 
			    profile[q[0][j]][r[0][0]]);

    diagonal_simd = _mm_add_ps(zero_simd, subst_simd);

    cmp_de = _mm_min_ps(_mm_cmpge_ps(diagonal_simd, e_simd), one_simd);
    max_de = _mm_max_ps(diagonal_simd, e_simd);

    // up value: gap in query
    f_simd = _mm_max_ps(_mm_sub_ps(zero_simd, gap_extend_simd), 
			_mm_sub_ps(zero_simd, gap_open_simd));

    cmp_fz = _mm_min_ps(_mm_cmpge_ps(f_simd, zero_simd), one_simd);
    max_fz = _mm_max_ps(f_simd, zero_simd);
    
    // get max. value and save it
    cmp_de_fz = _mm_min_ps(_mm_cmpge_ps(max_de, max_fz), one_simd);
    h_simd = _mm_max_ps(max_de, max_fz);

    score_simd = _mm_max_ps(score_simd, h_simd);
    
    // compass (save left, diagonal, up or zero?)
    c = _mm_slli_epi32(_mm_or_si128(zeroi, _mm_cvtps_epi32(cmp_de)), 1);
    c = _mm_slli_epi32(_mm_or_si128(c, _mm_cvtps_epi32(cmp_fz)), 1);
    c = _mm_or_si128(c, _mm_cvtps_epi32(cmp_de_fz));

    // update matrices
    _mm_store_ps(&H[j_depth], h_simd);
    _mm_store_ps(&F[j_depth], f_simd);
    _mm_store_si128(&C[j_depth], c);
  }

  for (int i = 1; i < max_r_len; i++) {
    
    h_simd = zero_simd;
    e_simd = zero_simd;
    temp_simd = zero_simd;

    idx = i * q_len_depth;

    for (int j = 0; j < max_q_len; j++) {
      j_depth = depth * j;
      offset = idx + j_depth;

      // left value: gap in reference
      e_simd = _mm_max_ps(_mm_sub_ps(e_simd, gap_extend_simd), 
			  _mm_sub_ps(h_simd, gap_open_simd));
 
      // diagonal value: match or mismatch
      diagonal_simd = _mm_add_ps(temp_simd,
				 _mm_set_ps((q_len[3] > j && r_len[3] > i) ? profile[q[3][j]][r[3][i]] : -1000.0f, 
					    (q_len[2] > j && r_len[2] > i) ? profile[q[2][j]][r[2][i]] : -1000.0f, 
					    (q_len[1] > j && r_len[1] > i) ? profile[q[1][j]][r[1][i]] : -1000.0f, 
					    (q_len[0] > j && r_len[0] > i) ? profile[q[0][j]][r[0][i]] : -1000.0f)
				 );

      cmp_de = _mm_min_ps(_mm_cmpge_ps(diagonal_simd, e_simd), one_simd);
      max_de = _mm_max_ps(diagonal_simd, e_simd);

      // up value: gap in query
      temp_simd = _mm_load_ps(&H[offset - q_len_depth]);

      f_simd = _mm_load_ps(&F[j_depth]);
      f_simd = _mm_max_ps(_mm_sub_ps(f_simd, gap_extend_simd), 
			  _mm_sub_ps(temp_simd, gap_open_simd));

      cmp_fz = _mm_min_ps(_mm_cmpge_ps(f_simd, zero_simd), one_simd);
      max_fz = _mm_max_ps(f_simd, zero_simd);

      // get max. value
      cmp_de_fz = _mm_min_ps(_mm_cmpge_ps(max_de, max_fz), one_simd);
      h_simd = _mm_max_ps(max_de, max_fz);

      score_simd = _mm_max_ps(score_simd, h_simd);

      // compass (save left, diagonal, up or zero?)
      c = _mm_slli_epi32(_mm_or_si128(zeroi, _mm_cvtps_epi32(cmp_de)), 1);
      c = _mm_slli_epi32(_mm_or_si128(c, _mm_cvtps_epi32(cmp_fz)), 1);
      c = _mm_or_si128(c, _mm_cvtps_epi32(cmp_de_fz));

      // update matrices
      _mm_store_ps(&H[offset], h_simd);
      _mm_store_ps(&F[j_depth], f_simd); 
      _mm_store_si128(&C[offset], c);
    }
  }
  _mm_store_ps(max_score, score_simd);
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
