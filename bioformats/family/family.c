#include "family.h"


/*
 * Individual management functions
 */

individual_t *individual_new(char *id, float phenotype, enum Sex sex, enum Condition condition, individual_t *father, individual_t *mother, family_t *family) {
    individual_t *individual = (individual_t*) malloc (sizeof(individual_t));
    individual_init(id, phenotype, sex, condition, father, mother, family, individual);
    return individual;
}

void individual_init(char *id, float phenotype, enum Sex sex, enum Condition condition, individual_t *father, individual_t *mother, family_t *family, individual_t *individual) {
    if (individual == NULL) {
        return;
    }
    
    individual->id = id;
    individual->phenotype = phenotype;
    individual->condition = condition;
    individual->sex = sex;
    individual->father = father;
    individual->mother = mother;
    individual->family = family;
}

void individual_free(individual_t *individual) {
    if (individual == NULL) {
        return;
    }
    
    free(individual->id);
    free(individual);
}

int individual_compare(individual_t *a, individual_t *b) {
    int result;
    if (!a || !b) {
        result = 1;
    } else if (a->family == NULL && b->family != NULL) {
        result = -1;
    } else if (a->family != NULL && b->family == NULL) {
        result = 1;
    } else {
        result = strcasecmp(a->id, b->id);
        LOG_DEBUG_F("1) %s:%s = %s:%s ? %d\n", a->family->id, a->id, b->family->id, b->id, !result);
        result |= strcasecmp(a->family->id, b->family->id);
        LOG_DEBUG_F("2) %s:%s = %s:%s ? %d\n", a->family->id, a->id, b->family->id, b->id, !result);
    }
    return result;
}


/*
 * Family management functions
 */

family_t *family_new(char *id) {
    family_t *family = (family_t*) calloc (1, sizeof(family_t));
    family->id = id;
    family->children = cp_list_create_list(COLLECTION_MODE_DEEP,
                                           (cp_compare_fn) individual_compare,
                                           NULL,
                                           (cp_destructor_fn) individual_free
                                          );
    family->unknown = cp_list_create_list(COLLECTION_MODE_DEEP,
                                           (cp_compare_fn) individual_compare,
                                           NULL,
                                           (cp_destructor_fn) individual_free
                                          );
    return family;
}

int family_set_parent(individual_t *parent, family_t *family) {
    if (!parent) {
        return -1;
    }
    if (!family) {
        return -2;
    }
    if (parent->sex == UNKNOWN_SEX) {
        return -3;
    }
    if (family_contains_individual(parent, family)) {
        return -4;
    }
    
    if (parent->sex == MALE) {
        if (family->father) {
            return 1;
        } else {
            family->father = parent;
        }
        assert(family->father);
    } else if (parent->sex == FEMALE) {
        if (family->mother) {
            return 2;
        } else {
            family->mother = parent;
        }
        assert(family->mother);
    }
    return 0;
}

int family_add_child(individual_t *child, family_t *family) {
    if (!child) {
        return -1;
    }
    if (!family) {
        return -2;
    }
    if (family_contains_individual(child, family)) {
        return -3;
    }
    
    assert(family->children);
    void *ret = cp_list_insert(family->children, child);
    return ret == NULL;
}

int family_add_unknown(individual_t *individual, family_t *family) {
    if (!individual) {
        return -1;
    }
    if (!family) {
        return -2;
    }
    if (family_contains_individual(individual, family)) {
        return -3;
    }
    
    assert(family->unknown);
    void *ret = cp_list_insert(family->unknown, individual);
    return ret == NULL;
}

void family_free(family_t *family) {
    if (family == NULL) {
        return;
    }
    
    free(family->id);
    individual_free(family->father);
    individual_free(family->mother);
    cp_list_destroy(family->children);
}

individual_t *family_contains_individual(individual_t *individual, family_t *family) {
    if (individual_compare(individual, family->father) == 0) {
        LOG_DEBUG_F("Individual %s:%s found as father\n", family->id, individual->id);
        return family->father;
    }
    if (individual_compare(individual, family->mother) == 0) {
        LOG_DEBUG_F("Individual %s:%s found as mother\n", family->id, individual->id);
        return family->mother;
    }
    
    cp_list_iterator *iterator = cp_list_create_iterator(family->children, COLLECTION_LOCK_READ);
    individual_t *child = NULL;
    while ((child = cp_list_iterator_next(iterator)) != NULL) {
        if (individual_compare(individual, child) == 0) {
            LOG_DEBUG_F("Individual %s:%s found as child\n", family->id, individual->id);
            cp_list_iterator_destroy(iterator);
            return child;
        }
    }
    cp_list_iterator_destroy(iterator);
    
    iterator = cp_list_create_iterator(family->unknown, COLLECTION_LOCK_READ);
    individual_t *unknown = NULL;
    while ((unknown = cp_list_iterator_next(iterator)) != NULL) {
        if (individual_compare(individual, unknown) == 0) {
            LOG_DEBUG_F("Individual %s:%s found as unknown member\n", family->id, individual->id);
            cp_list_iterator_destroy(iterator);
            return unknown;
        }
    }
    cp_list_iterator_destroy(iterator);
    
    return NULL;
}
