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
  TEST_ASSERT_EQUAL_INT32(1, n);
  TEST_ASSERT_EQUAL_STRING("pp-vs-past", weak[0]);
}

void test_same_day_does_not_expand_interval() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::grade(it,5,0);
  SrsScheduler::grade(it,5,0,true);
  TEST_ASSERT_EQUAL_UINT8(1,it.reps);
  TEST_ASSERT_EQUAL_UINT16(1,it.nextDay);
  TEST_ASSERT_FALSE(it.retained);
}
void test_failure_not_erased_by_immediate_retry() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::grade(it,1,0);
  SrsScheduler::grade(it,5,0,true);
  TEST_ASSERT_EQUAL_UINT8(0,it.reps);
  TEST_ASSERT_FALSE(it.retained);
}
void test_recall_on_later_day_and_ease_280() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::grade(it,5,0);
  SrsScheduler::grade(it,5,1,true);
  TEST_ASSERT_TRUE(it.retained);
  for(int i=0;i<20;++i)SrsScheduler::grade(it,5,it.nextDay,true);
  TEST_ASSERT_EQUAL_UINT16(280,it.easePct);
}

// ---- three-way recall self-assessment (gradeRecall) ----

void test_recall_recalled_is_full_success() {
  SrsItem it=SrsScheduler::makeItem("x",10);   // due on day 10
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Recalled,10);
  TEST_ASSERT_EQUAL_UINT8(1,it.reps);
  TEST_ASSERT_EQUAL_UINT8(0,it.lapses);
  TEST_ASSERT_EQUAL_UINT16(cfg::DAY_MIN,it.intervalDays);
  TEST_ASSERT_EQUAL_UINT16(11,it.nextDay);
  TEST_ASSERT_FALSE(it.retained);              // needs a later due day first
}

void test_recall_miss_is_full_lapse() {
  SrsItem it=SrsScheduler::makeItem("x",10);
  SrsScheduler::grade(it,5,10);                // seed a success first
  const uint16_t easeBefore=it.easePct;
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Miss,20);
  TEST_ASSERT_EQUAL_UINT8(0,it.reps);
  TEST_ASSERT_EQUAL_UINT8(1,it.lapses);
  TEST_ASSERT_EQUAL_UINT16(cfg::DAY_MIN,it.intervalDays);
  TEST_ASSERT_TRUE(it.easePct<easeBefore);
}

void test_recall_hint_is_softer_than_lapse() {
  SrsItem hint=SrsScheduler::makeItem("x",10);
  SrsScheduler::grade(hint,5,10);              // ease 250 -> scheduled
  SrsScheduler::gradeRecall(hint,(uint8_t)SrsScheduler::RecallOutcome::Hint,20);
  TEST_ASSERT_EQUAL_UINT8(0,hint.reps);        // not a success
  TEST_ASSERT_EQUAL_UINT8(0,hint.lapses);      // but not counted as a lapse
  TEST_ASSERT_EQUAL_UINT16(cfg::DAY_MIN,hint.intervalDays);  // retry tomorrow
  TEST_ASSERT_EQUAL_UINT16(21,hint.nextDay);   // interval did not grow
  TEST_ASSERT_EQUAL_UINT16(240,hint.easePct);  // half the miss penalty (250->240)

  SrsItem miss=SrsScheduler::makeItem("y",10);
  SrsScheduler::grade(miss,5,10);
  SrsScheduler::gradeRecall(miss,(uint8_t)SrsScheduler::RecallOutcome::Miss,20);
  TEST_ASSERT_TRUE(miss.easePct<hint.easePct); // miss punished harder
  TEST_ASSERT_EQUAL_UINT8(1,miss.lapses);
}

void test_recall_hint_same_day_is_practice() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::grade(it,5,0);                 // nextDay=1, lastDay=0
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Hint,0);  // same day
  TEST_ASSERT_EQUAL_UINT8(1,it.reps);
  TEST_ASSERT_EQUAL_UINT16(1,it.nextDay);
  TEST_ASSERT_EQUAL_UINT16(250,it.easePct);
}

void test_recall_hint_on_future_item_is_practice() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::grade(it,5,0);
  SrsScheduler::grade(it,5,1);                 // nextDay=7
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Hint,3);  // early
  TEST_ASSERT_EQUAL_UINT8(2,it.reps);
  TEST_ASSERT_EQUAL_UINT16(7,it.nextDay);
  TEST_ASSERT_EQUAL_UINT16(250,it.easePct);
}

void test_recall_miss_then_recall_same_day_stays_lapsed() {
  SrsItem it=SrsScheduler::makeItem("x",10);
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Miss,10);
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Recalled,10);
  TEST_ASSERT_EQUAL_UINT8(0,it.reps);          // failure is not erased
  TEST_ASSERT_EQUAL_UINT8(1,it.lapses);
}

void test_recall_recalled_on_later_due_day_is_retained() {
  SrsItem it=SrsScheduler::makeItem("x",0);
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Recalled,0);
  SrsScheduler::gradeRecall(it,(uint8_t)SrsScheduler::RecallOutcome::Recalled,1);
  TEST_ASSERT_TRUE(it.retained);               // independent recall on a later day
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_item_is_due_immediately);
  RUN_TEST(test_successful_sequence_grows_interval);
  RUN_TEST(test_failure_resets_and_shortens_interval);
  RUN_TEST(test_ease_clamped_to_minimum);
  RUN_TEST(test_mastery_ema_and_weak_tags);
  RUN_TEST(test_same_day_does_not_expand_interval);
  RUN_TEST(test_failure_not_erased_by_immediate_retry);
  RUN_TEST(test_recall_on_later_day_and_ease_280);
  RUN_TEST(test_recall_recalled_is_full_success);
  RUN_TEST(test_recall_miss_is_full_lapse);
  RUN_TEST(test_recall_hint_is_softer_than_lapse);
  RUN_TEST(test_recall_hint_same_day_is_practice);
  RUN_TEST(test_recall_hint_on_future_item_is_practice);
  RUN_TEST(test_recall_miss_then_recall_same_day_stays_lapsed);
  RUN_TEST(test_recall_recalled_on_later_due_day_is_retained);
  return UNITY_END();
}
