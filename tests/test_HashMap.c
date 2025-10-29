#include <limits.h>
#include <string.h> /* memset */

#include "HashMap.h"
#include "tau/tau.h"

TAU_MAIN()

/*###################################################################*/
/*######################### hashmap_creation ########################*/
/*###################################################################*/

struct hashmap_creation
{
	HashMap_int *restrict output;
};

TEST_F_SETUP(hashmap_creation) { memset(tau, 0, sizeof(*tau)); }

TEST_F_TEARDOWN(hashmap_creation)
{
	tau->output = hm_int_delete(tau->output, NULL);
}

TEST(hashmap_creation, test_invalid_capacities)
{
	CHECK_PTR_EQ(hm_int_new(-1), NULL);
	CHECK_PTR_EQ(hm_int_new(0), NULL);
	CHECK_PTR_EQ(hm_int_new(LEN_TY_min), NULL);
	CHECK_PTR_EQ(hm_int_new(LEN_TY_max), NULL);
}

TEST_F(hashmap_creation, test_creation)
{
	tau->output = hm_int_new(1);
	REQUIRE_PTR_NE(tau->output, NULL);
	CHECK(tau->output->capacity == 1);
	CHECK(tau->output->used == 0);
	CHECK_PTR_NE(tau->output->arr, NULL);
	/* memory checkers should not complain */
	tau->output->arr[tau->output->capacity - 1].hash = 69;
}

TEST_F(hashmap_creation, test_cellar_is_initialised)
{
	const len_ty capacity = 64;
#ifdef CELLAR_COALESCED_HASHING
	const len_ty cellar_capacity = 10;
#endif /* CELLAR_COALESCED_HASHING */

	tau->output = hm_int_new(capacity);
	REQUIRE_PTR_NE(tau->output, NULL);
	CHECK(tau->output->capacity == capacity);
	CHECK(tau->output->used == 0);
	CHECK_PTR_NE(tau->output->arr, NULL);
	/* memory checkers should not complain */
	tau->output->arr[tau->output->capacity - 1].hash = 69;

#ifdef CELLAR_COALESCED_HASHING
	CHECK(tau->output->cellar.capacity == cellar_capacity);
	CHECK(tau->output->cellar.used == 0);
	/* memory checkers should not complain */
	tau->output->arr[capacity + cellar_capacity - 1].hash = 69;
#endif /* CELLAR_COALESCED_HASHING */
}

// TEST_F(hashmap_creation, test_capacity_at_limit)
// {
// 	const len_ty capacity = LEN_TY_max - sizeof(HashMap) -
// 							(((LEN_TY_max - sizeof(HashMap)) * 14) / 100);

// 	tau->output = hm_int_new(capacity);
// 	REQUIRE_PTR_NE(tau->output, NULL);
// 	CHECK(tau->output->capacity == capacity);
// 	CHECK(tau->output->used == 0);
// 	CHECK_PTR_NE(tau->output->arr, NULL);
// 	/* memory checkers should not complain */
// 	tau->output->arr[tau->output->capacity - 1].hash = 69;
// }

/*###################################################################*/
/*########################## hashing ################################*/
/*###################################################################*/

TEST(hashing, test_invalid_inputs)
{
	hash_ty hash;

	/* clang-format off */
	CHECK(hm_int_hash(NULL, (u8mem){.len = -1, .buf = NULL}) == false);
	CHECK(hm_int_hash(NULL, (u8mem){.len = -1, .buf = (unsigned char *)1}) == false);
	CHECK(hm_int_hash(NULL, (u8mem){.len = 1, .buf = NULL}) == false);
	CHECK(hm_int_hash(NULL, (u8mem){.len = 1, .buf = (unsigned char *)1}) == false);
	CHECK(hm_int_hash(&hash, (u8mem){.len = -1, .buf = (unsigned char *)1}) == false);
	CHECK(hm_int_hash(&hash, (u8mem){.len = 0, .buf = (unsigned char *)1}) == false);
	CHECK(hm_int_hash(&hash, (u8mem){.len = 1, .buf = NULL}) == false);
	CHECK(hm_int_hash(&hash, (u8mem){.len = 0, .buf = NULL}) == false);
	CHECK(hm_int_hash(&hash, (u8mem){.len = -1, .buf = NULL}) == false);
	/* clang-format on */
}

TEST(hashing, test_same_input_same_output)
{
	unsigned char mem[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 123};
	hash_ty h1, h2;

	/* clang-format off */
	REQUIRE(hm_int_hash(&h1, (u8mem){.len=sizeof(mem)/sizeof(*mem), .buf=mem}) == true);
	REQUIRE(hm_int_hash(&h2, (u8mem){.len=sizeof(mem)/sizeof(*mem), .buf=mem}) == true);
	/* clang-format on */
	CHECK(h1 == h2);
}

/*###################################################################*/
/*######################## hashmap_searching ########################*/
/*###################################################################*/

TEST(hashmap_searching, test_invalid_hashmap)
{
	/* clang-format off */
	CHECK_PTR_EQ(hm_int_search(&(HashMap_int){.capacity = -1, .used = -1}, (u8mem){.len = 1, .buf = (unsigned char *)1}), NULL);
	CHECK_PTR_EQ(hm_int_search(&(HashMap_int){.capacity = 0, .used = -1}, (u8mem){.len = 1, .buf = (unsigned char *)1}), NULL);
	CHECK_PTR_EQ(hm_int_search(&(HashMap_int){.capacity = 1, .used = -1}, (u8mem){.len = 1, .buf = (unsigned char *)1}), NULL);
	CHECK_PTR_EQ(hm_int_search(&(HashMap_int){.capacity = -1, .used = 0}, (u8mem){.len = 1, .buf = (unsigned char *)1}), NULL);
	CHECK_PTR_EQ(hm_int_search(&(HashMap_int){.capacity = 0, .used = 0}, (u8mem){.len = 1, .buf = (unsigned char *)1}),	NULL);
	/* clang-format on */
}

/*###################################################################*/
/*############################## expanding ##########################*/
/*###################################################################*/

struct expanding
{
	HashMap_int *restrict input;
};

TEST_F_SETUP(expanding)
{
	tau->input = hm_int_new(16);
	REQUIRE_PTR_NE(tau->input, NULL);
}

TEST_F_TEARDOWN(expanding) { tau->input = hm_int_delete(tau->input, NULL); }

TEST(expanding, test_invalid_arguments)
{
	CHECK_PTR_EQ(hm_int_grow(NULL, -1), NULL);
	CHECK_PTR_EQ(hm_int_grow(NULL, 0), NULL);
	CHECK_PTR_EQ(hm_int_grow(NULL, 1), NULL);
	/* clang-format off */
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = -1, .used = -1}, 1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 0, .used = -1}, 1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = -1}, 1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = 0}, -1), NULL);

#ifdef CELLAR_COALESCED_HASHING
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = 0, .cellar = (Cellar_int){.capacity = -1, .used = -1}}, -1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = 0, .cellar = (Cellar_int){.capacity = 0, .used = -1}}, -1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = 0, .cellar = (Cellar_int){.capacity = 1, .used = -1}}, -1), NULL);
	CHECK_PTR_EQ(hm_int_grow(&(HashMap_int){.capacity = 1, .used = 0, .cellar = (Cellar_int){.capacity = 1, .used = 1}}, -1), NULL);
	/* clang-format on */
#endif /* CELLAR_COALESCED_HASHING */
}

TEST(expanding, test_no_growth)
{
	HashMap_int input = {.capacity = 8};

	CHECK_PTR_EQ(hm_int_grow(&input, input.capacity / 2), &input);
	CHECK_PTR_EQ(hm_int_grow(&input, input.capacity), &input);
}

TEST_F(expanding, test_growing_a_hashmap)
{
	const len_ty initial_capacity = tau->input->capacity;
#ifdef CELLAR_COALESCED_HASHING
	const len_ty initial_cellar_capacity = tau->input->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	HashMap_int *const restrict ouput =
		hm_int_grow(tau->input, tau->input->capacity * 2);

	REQUIRE_PTR_NE(ouput, NULL);
	tau->input = ouput;
	CHECK(tau->input->capacity == initial_capacity * 2);
#ifdef CELLAR_COALESCED_HASHING
	CHECK(tau->input->cellar.capacity >= initial_cellar_capacity * 2);
#endif /* CELLAR_COALESCED_HASHING */
}
