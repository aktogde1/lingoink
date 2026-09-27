// Host integration coverage for actual firmware screens, parser and SD format.
#include "screens/LessonScreen.h"
#include "screens/DateScreen.h"
#include "screens/HomeScreen.h"
#include "screens/ProgressScreen.h"
#include "bmp_out.h"
#include <cassert>
#include <string>
#include <iostream>
#include <fstream>

struct StudyTests {
  static inline Canvas c;
  static inline CourseCatalog catalog;
  static inline ProgressStore p;
  static inline LessonScreen ls;
  static void snap(const char* name) {
    c.fillWhite();ls.render(c);
    char path[128];snprintf(path,sizeof(path),"out/preview/%s.bmp",name);
    bmp::writeScene(path,c); // replaced below with the host helper's actual signature
  }
  static void tick(Key k) {ls.handleKey(k);ls.checkpoint();}
  static void answer(bool correct=true) {
    auto& e=ls.cur();
    if(e.type==ExType::Recall) {
      // Three outcomes: sel_ 0 = NOT YET, 1 = WITH HINT, 2 = RECALLED.
      if(!ls.revealed_)tick(Key::Ok);
      if(correct) {tick(Key::Down);tick(Key::Down);}
      tick(Key::Ok);
    } else {
      uint8_t target=correct?e.correct:(e.correct+1)%e.optionCount;
      for(int i=0;ls.order_[ls.sel_]!=target && i<10;++i)tick(Key::Down);
      tick(Key::Ok);
    }
  }
  static void exercise() {
    for(int i=0;ls.phase()!=LessonScreen::Phase::Exercise && i<300;++i)tick(Key::Ok);
    assert(ls.phase()==LessonScreen::Phase::Exercise);
  }
  static void finish() {
    for(int guard=0;ls.phase()!=LessonScreen::Phase::Summary && guard<600;++guard) {
      if(ls.phase()==LessonScreen::Phase::Exercise && !ls.answered_)answer();else tick(Key::Ok);
    }
    assert(ls.phase()==LessonScreen::Phase::Summary);
  }
  static std::string normalize(const std::string& text) {
    std::string out;for(unsigned char ch:text)if(ch!=' ' && ch!='\n' && ch!='\r')out+=ch;return out;
  }
  static void contentAndReading() {
    int questions=0,recall=0,reading=0,missing=0;
    for(uint8_t n=0;n<catalog.course.lessonCount;++n) {
      p.session=StudySession{};
      assert(ls.start(catalog.course.lessons[n],&p,&c));
      assert(ls.lesson_.theoryCount<=4);
      for(uint8_t e=0;e<ls.lesson_.exerciseCount;++e) {
        auto& x=ls.lesson_.exercises[e];
        if(x.type!=ExType::Reading) {
          ++questions;if(x.type==ExType::Recall)++recall;
          if(!x.explain || !x.explain[0])++missing;
          assert(x.correct<x.optionCount);
          continue;
        }
        ++reading;
        for(int orientation=0;orientation<4;++orientation) {
          c.setOrientation(orientation);
          for(uint8_t pg=0;pg<x.pageCount;++pg) {
            uint16_t at=0;std::string joined;
            do {
              uint16_t next=ls.textEnd(x.pages[pg],at,&c);
              assert(next>at);joined.append(x.pages[pg]+at,next-at);at=next;
            }while(x.pages[pg][at]);
            assert(normalize(joined)==normalize(x.pages[pg]));
            uint8_t page=pg;uint16_t offset=at;
            assert(ls.textBack(x,page,offset));assert(offset<at);
          }
        }
      }
    }
    c.setOrientation(3);
    assert(missing==0 && recall==42 && reading==7);
    std::cout<<"Content: "<<questions<<" questions, "<<recall<<" recall, "<<reading<<" texts; all prose covered in 4 orientations\n";
  }
  static void scheduler() {
    auto it=SrsScheduler::makeItem("x",0);
    SrsScheduler::grade(it,5,0);SrsScheduler::grade(it,5,0);
    assert(it.reps==1 && it.nextDay==1);
    SrsScheduler::grade(it,1,0);SrsScheduler::grade(it,5,0);
    assert(it.reps==0 && !it.retained);
    SrsScheduler::grade(it,5,1,true);assert(it.reps==1);
    SrsScheduler::grade(it,5,2,true);assert(it.retained && it.reps==2);
    for(int n=0;n<20;++n)SrsScheduler::grade(it,5,it.nextDay,true);
    assert(it.easePct==280 && it.intervalDays<=365);
    SrsScheduler::grade(it,1,it.nextDay);assert(!it.retained);
    Mastery m;for(int i=0;i<40;++i) {char tag[32];snprintf(tag,sizeof(tag),"topic-%d",i);const char* ts[]={tag};for(int z=0;z<3;++z)m.record(Skill::Grammar,ts,1,false);}
    assert(m.tags[39].used);
    std::cout<<"Scheduler: same-day protection, failure wins, recall retention, ease cap, 40 tags OK\n";
  }
  static void calendarAndMigration() {
    ProgressStore old;
    const char* legacy=R"({"course":"english_ru","day":7,"done":17,"lastLesson":"a2_05","srs":[{"id":"pp.form","e":4,"nd":8}]})";
    assert(old.parse(legacy,strlen(legacy)));old.bindCourse(catalog);
    assert(old.stage("a2_01")==1 && old.stage("a2_05")==1 && old.stage("a2_11")==0);
    assert(old.items[0].easePct==250);
    assert(old.setDate(20260228));assert(old.time.day==7);
    old.addMinutes(1440);assert(old.time.day==7);
    assert(old.setDate(20260301));assert(old.time.day==8);
    assert(!old.setDate(20260228));assert(!old.setDate(20260229));
    assert(dateOrdinal(20240301)-dateOrdinal(20240228)==2);
    assert(strcmp(old.nextLesson(catalog)->id,"a2_11")==0);
    strcpy(old.lastLessonId,"b1_21");assert(strcmp(old.nextLesson(catalog)->id,"a2_11")==0);
    std::cout<<"Date and migration: elapsed calendar days, leap years, stable lesson IDs, forward navigation OK\n";
  }
  static void lessonFlow() {
    p.reset();p.bindCourse(catalog);p.setDate(20260926);
    assert(ls.start(*catalog.findLesson("a2_01"),&p,&c));exercise();
    const uint8_t first=ls.exIdx_;answer(false);
    assert(ls.answerCount_==1 && ls.unresolvedCount()==1 && ls.study_.length==ls.lesson_.exerciseCount+1);
    ls.checkpoint();
    assert(ls.start(*catalog.findLesson("a2_01"),&p,&c));
    assert(ls.answered_ && ls.answerCount_==1 && ls.exIdx_==first);
    tick(Key::Ok);tick(Key::OkLong);assert(ls.helped_);tick(Key::Back);answer();
    assert(ls.correctCount_==0); // correct only after opening explanation
    tick(Key::Ok);finish();assert(ls.answerCount_==8 && ls.correctCount_==6);
    assert(ls.unresolvedCount()==0);
    snap("learning_summary_o3");
    // Resume a completed summary rather than repeating the lesson.
    ls.checkpoint();assert(ls.start(*catalog.findLesson("a2_01"),&p,&c));assert(ls.phase()==LessonScreen::Phase::Summary);
    // New day: review chooses learned due items and favours new-context recall.
    p.session=StudySession{};p.setDate(20260927);
    assert(ls.startReview(*catalog.findLesson("a2_01"),&p,&c));assert(ls.isReview());
    assert(ls.cur().type==ExType::Recall);
    const char* recallId=ls.cur().srsIds[0];
    snap("learning_recall_hidden_o3");tick(Key::Ok);snap("learning_recall_shown_o3");
    // An honest HINT outcome: not a success, not a lapse — the item retries
    // tomorrow (nextDay = today+1) and stays out of today's due queue.
    tick(Key::Down);tick(Key::Ok);tick(Key::Ok);
    auto* hinted=p.find(recallId);
    finish();
    assert(hinted && hinted->nextDay==(uint16_t)(p.time.day+1) && hinted->lapses==0);
    // Day two: the hinted item is due again; an independent recall confirms it.
    p.session=StudySession{};p.setDate(20260928);
    assert(ls.startReview(*catalog.findLesson("a2_01"),&p,&c));
    assert(ls.cur().type==ExType::Recall && ls.cur().srsIds[0]==std::string(recallId));
    tick(Key::Ok);tick(Key::Down);tick(Key::Down);tick(Key::Ok);tick(Key::Ok);
    assert(p.find(recallId)->retained);
    assert(p.confirmedCount()>0);
    p.session=StudySession{};
    assert(ls.start(*catalog.findLesson("b1_21"),&p,&c));
    for(int i=0;ls.phase()!=LessonScreen::Phase::Reading && i<100;++i)tick(Key::Ok);
    for(int i=0;!ls.readingOffset_ && ls.phase()==LessonScreen::Phase::Reading && i<20;++i)tick(Key::Ok);
    assert(ls.readingOffset_>0);
    uint16_t savedOffset=ls.readingOffset_;assert(ls.start(*catalog.findLesson("b1_21"),&p,&c));assert(ls.readingOffset_==savedOffset);
    snap("learning_reading_o3");exercise();
    uint16_t question=ls.exIdx_;tick(Key::Back);tick(Key::Down);tick(Key::Ok);
    assert(ls.referenceMode_==2);snap("learning_text_reference_o3");
    tick(Key::Back);tick(Key::Down);tick(Key::Ok);assert(ls.referenceMode_==3);snap("learning_dictionary_o3");
    tick(Key::Back);tick(Key::Back);assert(ls.exIdx_==question && !ls.answered_);
    // Reading lookup does not change a comprehension answer into a memory quiz.
    std::cout<<"Learning flow: retries, assisted answers, feedback/summary/reading resume, review, text and dictionary return OK\n";
  }
  static void reviewQueue() {
    p.reset();p.bindCourse(catalog);p.setDate(20260926);
    // Seed due items from three different lessons, as studied lessons would.
    p.findOrCreate("vp.borrow");    // a2_01: recall + quiz -> dedupe keeps the recall
    p.findOrCreate("vp.actually");  // a2_01: recall + quiz
    p.findOrCreate("ps.went");      // a2_05 quizzes
    p.findOrCreate("pv.look-for");  // b1_20 quiz
    p.session=StudySession{};
    uint8_t qL[12],qE[12];
    const uint8_t n=ls.buildReviewQueue(catalog,&p,qL,qE,12);
    assert(n==4);
    uint8_t distinct=0;bool seenL[24]={};
    for(uint8_t i=0;i<n;++i) if(!seenL[qL[i]]){seenL[qL[i]]=true;++distinct;}
    assert(distinct==3 && qL[0]==catalog.indexOf("a2_01"));
    StudySession fresh;fresh.review=true;fresh.qLen=n;
    for(uint8_t i=0;i<n;++i){fresh.qLesson[i]=qL[i];fresh.qEx[i]=qE[i];}
    p.session=fresh;
    uint8_t count=1;while(count<n && qL[count]==qL[0]) ++count;
    assert(ls.startReviewSlice(*catalog.findLesson("a2_01"),count,&p,&c,true));
    ls.checkpoint();
    assert(ls.isReview() && ls.phase()==LessonScreen::Phase::Exercise);
    assert(p.session.qPos==count);
    // Run the queue to the end, chaining across lessons without the menu.
    uint8_t slices=1;
    for(;;) {
      for(int guard=0;ls.phase()!=LessonScreen::Phase::Summary && guard<600;++guard) {
        if(ls.phase()==LessonScreen::Phase::Exercise && !ls.answered_)answer();else ls.handleKey(Key::Ok);
        ls.checkpoint();
      }
      assert(ls.phase()==LessonScreen::Phase::Summary);
      uint8_t cc=0;
      const LessonMeta* ch=ls.takeChain(&catalog,&cc);
      if(!ch) break;
      assert(ls.startReviewSlice(*ch,cc,&p,&c,false));++slices;
    }
    assert(slices==3 && ls.isReview() && p.session.qPos==n);
    assert(p.session.qAnswers==n && p.session.qUnresolved==0);
    // The bookmark round-trips (power-off mid-review restores the queue).
    SD.allowWrites=true;
    assert(p.save());
    ProgressStore r2;assert(r2.load());
    assert(r2.session.qLen==n && r2.session.qPos==n && r2.session.qAnswers==n);
    SD.allowWrites=false;
    std::cout<<"Review queue: cross-lesson build, recall-first, chaining, queue bookmark OK\n";
  }
  static void storage() {
    SD.allowWrites=true;SD.mkdir(cfg::DATA_DIR);
    assert(p.save());ProgressStore restored;assert(restored.load());
    assert(restored.session.fingerprint==p.session.fingerprint);
    assert(restored.session.cursor==p.session.cursor);
    assert(restored.session.first==p.session.first);
    assert(restored.itemCount==p.itemCount && restored.calendarDate==p.calendarDate);
    assert(p.save()); // second complete copy becomes recovery file
    {std::ofstream bad(SDClass::mapPath(cfg::PROGRESS_FILE));bad<<"broken json";}
    ProgressStore recovered;assert(recovered.load());assert(recovered.itemCount==p.itemCount);
    assert(p.save());SD.allowWrites=false;
    std::cout<<"Storage: session and SRS round trip, recovery from invalid primary file OK\n";
  }
  static void run() {
    c.init(3);Strings::setLang(1);assert(catalog.begin());assert(catalog.course.lessonCount==21);
    scheduler();calendarAndMigration();contentAndReading();lessonFlow();reviewQueue();storage();
    DateScreen date;date.start(&p);c.fillWhite();date.render(c);
    for(uint8_t o=0;o<4;++o) {
      c.setOrientation(o);char name[96];
      c.fillWhite();date.render(c);snprintf(name,sizeof(name),"out/preview/learning_date_o%u.bmp",o);bmp::writeScene(name,c);
      ProgressScreen stats;stats.bind(&p);c.fillWhite();stats.render(c);snprintf(name,sizeof(name),"out/preview/learning_progress_o%u.bmp",o);bmp::writeScene(name,c);
    }
    c.setOrientation(3);
    // host preview output helper
    std::cout<<"All learning integration checks passed.\n";
  }
};
int main(){std::cout<<std::unitbuf;StudyTests::run();}

