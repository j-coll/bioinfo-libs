/*
 * Copyright (c) 2013 Cristina Yenyxe Gonzalez Garcia (ICM-CIPF)
 * Copyright (c) 2013 Ignacio Medina (ICM-CIPF)
 *
 * This file is part of bioinfo-libs.
 *
 * bioinfo-libs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * bioinfo-libs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bioinfo-libs. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VARIANT_EFFECT_H
#define VARIANT_EFFECT_H

#include <stdlib.h>

#include <curl/curl.h>
#include <omp.h>

#include <bioformats/vcf/vcf_file_structure.h>
#include <commons/http_utils.h>
#include <commons/log.h>
#include <commons/string_utils.h>


// Line buffers and their maximum size (one per thread)
char **effect_line, **snp_line, **mutation_line;
int *max_line_size, *snp_max_line_size, *mutation_max_line_size;


/* **********************************************
 *              Web service request             *
 * **********************************************/

/**
 * @brief Invokes the effect web service for a list of regions.
 * 
 * @param url URL to invoke the web service through
 * @param records VCF records whose variant effect will be predicted
 * @param num_regions number of regions
 * @param excludes consequence types to exclude from the response
 * @return Whether the request could be successfully serviced
 * 
 * Given a list of genome positions, invokes the web service that returns a list of effect or consequences 
 * of the mutations in them. A callback function in order to parse the response.
 */
int invoke_effect_ws(const char *url, vcf_record_t **records, int num_records, char *excludes);

int invoke_snp_phenotype_ws(const char *url, vcf_record_t **records, int num_records);

int invoke_mutation_phenotype_ws(const char *url, vcf_record_t **records, int num_records);

static size_t save_effect_response(char *contents, size_t size, size_t nmemb, void *userdata);

static size_t save_snp_phenotype_response(char *contents, size_t size, size_t nmemb, void *userdata);

static size_t save_mutation_phenotype_response(char *contents, size_t size, size_t nmemb, void *userdata);



void initialize_ws_buffers(int num_threads);

void free_ws_buffers(int num_threads);

#endif
