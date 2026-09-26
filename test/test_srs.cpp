#include <unity.h>

#include "../src/srs/SrsScheduler.h"
#include "../src/srs/Mastery.h"

void setUp() {}
void tearDown() {}

void test_new_item_is_due_immediately() {
  SrsItem it = SrsScheduler::makeItem("vp.actually", 10);
  TEST_ASSERT_TRUE(SrsScheduler::isDue(it, 10));
  TEST_ASSERT_EQUAL_UINT16(0, it.intervalDays);
}

void test_successful_sequence_grows_interval() {
  SrsItem it = SrsScheduler::makeItem("pp.form", 0);
  SrsScheduler::grade(it, 5, 0);          // reps 1 -> 1 day
  TEST_ASSERT_EQUAL_UINT16(1, it.intervalDays);
  TEST_ASSERT_EQUAL_UINT16(1, it.nextDay);
  SrsScheduler::grade(it, 5, 1);          // reps 2 -> 6 days
  TEST_ASSERT_EQUAL_UINT16(6, it.intervalDays);
  SrsScheduler::grade(it, 5, 7);          // reps 3 -> 6 * ease
  TEST_ASSERT_TRUE(it.intervalDays > 6);
  TEST_ASSERT_EQUAL_UINT16(it.intervalDays, it.nextDay - 7);
}

void test_failure_resets_and_shortens_interval() {
  SrsItem it = SrsScheduler::makeItem("x", 0);
  SrsScheduler::grade(it, 5, 0);
  SrsScheduler::grade(it, 5, 1);
  SrsScheduler::grade(it, 5, 7);
  uint16_t grown = it.intervalDays;
  SrsScheduler::grade(it, 1, 20);         // fail
  TEST_ASSERT_EQUAL_UINT16(0, it.reps);
  TEST_ASSERT_EQUAL_UINT8(1, it.lapses);
  TEST_ASSERT_EQUAL_UINT16(cfg::DAY_MIN, it.intervalDays);
  TEST_ASSERT_TRUE(it.intervalDays < grown);
  TEST_ASSERT_EQUAL_UINT16(20 + cfg::DAY_MIN, it.nextDay);
}

void test_ease_clamped_to_minimum() {
  SrsItem it = SrsScheduler::makeItem("x", 0);
  for (int i = 0; i < 20; i++) {
    SrsScheduler::grade(it, 0, (uint16_t)i); // repeated hard failures
  }
  TEST_ASSERT_TRUE(it.easePct >= (uint8_t)(cfg::EASE_MIN * 100));
}

void test_mastery_ema_and_weak_tags() {
  Mastery m;
  m.record(Skill::Grammar, nullptr, 0, false);
  m.record(Skill::Grammar, nullptr, 0, false);
  m.record(Skill::Grammar, nullptr, 0, false);
  m.record(Skill::Grammar, nullptr, 0, false);
  m.record(Skill::Grammar, nullptr, 0, false);
  TEST_ASSERT_LESS_THAN_UINT8(30, m.skills[(size_t)Skill::Grammar].percent());

  const char* tags1[] = {"pp-vs-past"};
  const char* tags2[] = {"articles"};
  for (int i = 0; i < 6; i++) {
    m.record(Skill::Grammar, tags1, 1, false);
    m.record(Skill::Grammar, tags2, 1, true);
  }
  const char* weak[2];
  int n = m.weakTags(weak, 2);
  TEST_ASSERT_EQUAL_INT32(2, n);
  TEST_ASSERT_EQUAL_STRING("pp-vs-past", weak[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_item_is_due_immediately);
  RUN_TEST(test_successful_sequence_grows_interval);
  RUN_TEST(test_failure_resets_and_shortens_interval);
  RUN_TEST(test_ease_clamped_to_minimum);
  RUN_TEST(test_mastery_ema_and_weak_tags);
  return UNITY_END();
}
