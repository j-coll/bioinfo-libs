#include <stdlib.h>
#include <check.h>

#include <bioformats/features/region/region.h>

Suite *create_test_suite();


char **region_ordering;
int num_chromosomes;


/* ******************************
 * 	Unchecked fixtures	*
 * ******************************/

void setup_comparison(void) {
    region_ordering = get_chromosome_order("http://ws.bioinfo.cipf.es/", "hsa", "latest", &num_chromosomes);
}

void teardown_comparison(void)
{
    for (int i = 0; i < 25; i++) {
            free(region_ordering[i]);
    }
    free(region_ordering);
}


/* ******************************
 * 	     Unit tests	        *
 * ******************************/

START_TEST (null_order_list) {
    int num_chromosomes;
    char **ordering;
    
    ordering = get_chromosome_order(NULL, "hsa", "latest", &num_chromosomes);
    fail_if(ordering, "Chromosome order must not be constructed with a null URL");
    
    ordering = get_chromosome_order("http://ws.bioinfo.cipf.es/", NULL, "latest", &num_chromosomes);
    fail_if(ordering, "Chromosome order must not be constructed with a null species");
    
    ordering = get_chromosome_order("http://ws.bioinfo.cipf.es/", "hsa", NULL, &num_chromosomes);
    fail_if(ordering, "Chromosome order must not be constructed with a null version");
    
    ordering = get_chromosome_order(NULL, NULL, NULL, &num_chromosomes);
    fail_if(ordering, "Chromosome order must not be constructed with all arguments set to NULL");
}
END_TEST


START_TEST (create_order_list) {
    char **ordering;

    // Test by default list
    ordering = get_chromosome_order("http://ws.bioinfo.cipf.es/", "hsa", "latest", &num_chromosomes);
    fail_unless(num_chromosomes == 25, "The number of chromosomes for HSA is 25");

    for (int i = 0; i < 22; i++) {
        fail_unless(atoi(ordering[i]) == (i+1), 
                    "The former part of the list of chromosomes must contain all numerical ones");
    }
    fail_unless(strcmp(ordering[22], "X") == 0, 	"Chromosome 22 must be X");
    fail_unless(strcmp(ordering[23], "Y") == 0, 	"Chromosome 23 must be Y");
    fail_unless(strcmp(ordering[24], "MT") == 0, 	"Chromosome 24 must be MT");

    for (int i = 0; i < 25; i++) {
        free(ordering[i]);
    }
    free(ordering);
}
END_TEST


START_TEST (compare_diff_args)
{
    region_t reg_1, reg_2, reg_3, reg_4;

    reg_1.chromosome = "2";
    reg_1.start_position = 10000;
    reg_1.end_position = 20000;

    reg_2.chromosome = "4";
    reg_2.start_position = 5000;
    reg_2.end_position = 7000;

    reg_3.chromosome = "4";
    reg_3.start_position = 15000;
    reg_3.end_position = 27000;

    reg_4.chromosome = "X";
    reg_4.start_position = 3000;
    reg_4.end_position = 8000;


    fail_unless(compare_regions(&reg_1, &reg_2, region_ordering, num_chromosomes) < 0,
                "Region 2:10000-20000 must be considered less than 4:5000-7000");
    fail_unless(compare_regions(&reg_1, &reg_3, region_ordering, num_chromosomes) < 0,
                "Region 2:10000-20000 must be considered less than 4:15000-27000");
    fail_unless(compare_regions(&reg_1, &reg_4, region_ordering, num_chromosomes) < 0,
                "Region 2:10000-20000 must be considered less than X:3000-8000");

    fail_unless(compare_regions(&reg_2, &reg_3, region_ordering, num_chromosomes) < 0,
                "Region 4:5000-7000 must be considered less than 4:15000-17000");
    fail_unless(compare_regions(&reg_2, &reg_4, region_ordering, num_chromosomes) < 0,
                "Region 4:5000-27000 must be considered less than X:3000-8000");

    fail_unless(compare_regions(&reg_3, &reg_4, region_ordering, num_chromosomes) < 0,
                "Region 4:1500-27000 must be considered less than X:3000-8000");
}
END_TEST


START_TEST (compare_same_chr) {
    region_t reg_1, reg_2, reg_3;

    reg_1.chromosome = "4";
    reg_1.start_position = 1000;
    reg_1.end_position = 2000;

    reg_2.chromosome = "4";
    reg_2.start_position = 5000;
    reg_2.end_position = 27000;

    reg_3.chromosome = "4";
    reg_3.start_position = 16000;
    reg_3.end_position = 17000;


    fail_unless(compare_regions(&reg_1, &reg_2, region_ordering, num_chromosomes) < 0,
                "Region 4:1000-2000 must be considered less than 4:5000-27000");
    fail_unless(compare_regions(&reg_1, &reg_3, region_ordering, num_chromosomes) < 0,
                "Region 4:1000-2000 must be considered less than 4:16000-17000");
    fail_unless(compare_regions(&reg_2, &reg_3, region_ordering, num_chromosomes) < 0,
                "Region 4:5000-27000 must be considered less than 4:16000-17000");
}
END_TEST


START_TEST (compare_same_chr_same_start_position) {
    region_t reg_1, reg_2, reg_3;

    reg_1.chromosome = "2";
    reg_1.start_position = 10000;
    reg_1.end_position = 20000;

    reg_2.chromosome = "2";
    reg_2.start_position = 10000;
    reg_2.end_position = 20000;

    reg_3.chromosome = "2";
    reg_3.start_position = 10000;
    reg_3.end_position = 15000;

    fail_unless(compare_regions(&reg_1, &reg_2, region_ordering, num_chromosomes) == 0,
                "Regions 2:10000-20000 and 2:10000-20000 must be considered equal");
    fail_unless(compare_regions(&reg_2, &reg_3, region_ordering, num_chromosomes) == 0,
                "Region 2:10000-20000 and 2:10000-15000 must be considered equal");
}
END_TEST



/* ******************************
 * 	Main entry point	*
 * ******************************/


int main(int argc, char *argv[]) {
    Suite *fs = create_test_suite();
    SRunner *fs_runner = srunner_create(fs);
    srunner_run_all(fs_runner, CK_NORMAL);
    int number_failed = srunner_ntests_failed(fs_runner);
    srunner_free (fs_runner);

    return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

Suite *create_test_suite() {
    // Creation of the list defining the order
    TCase *tc_list = tcase_create("List creation");
    tcase_add_test(tc_list, null_order_list);
    tcase_add_test(tc_list, create_order_list);

    // Creation of an ordering list
    TCase *tc_comparison = tcase_create("Region comparison");
    tcase_add_unchecked_fixture(tc_comparison, setup_comparison, teardown_comparison);
    tcase_add_test(tc_comparison, compare_diff_args);
    tcase_add_test(tc_comparison, compare_same_chr);
    tcase_add_test(tc_comparison, compare_same_chr_same_start_position);

    // Add test cases to a test suite
    Suite *fs = suite_create("Region ordering");
    suite_add_tcase(fs, tc_list);
    suite_add_tcase(fs, tc_comparison);

    return fs;
}
