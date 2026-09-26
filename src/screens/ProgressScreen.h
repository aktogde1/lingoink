#pragma once
#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/Strings.h"
#include "../progress/ProgressStore.h"
class ProgressScreen:public Screen {
 public:
  void bind(ProgressStore* p){progress_=p;}
  const char* takePractice(){bool a=practice_;practice_=false;return a?tag_:nullptr;}
  void render(Canvas& c) override {
    auto mt=chrome::metrics(c);auto body=fontByRole(FontRole::Body);auto ui=fontByRole(FontRole::UI);
    chrome::header(c,mt,U("PRACTICE","ПРАКТИКА"));
    const StrId labels[]={SkVocabulary,SkGrammar,SkReading};int y=mt.top;
    for(int i=0;i<3;++i) {
      const auto& st=progress_->mastery.skills[i];char v[48];
      if(st.count) snprintf(v,sizeof(v),U("%u%% / %u attempts","%u%% / %u ответов"),st.percent(),st.count);
      else snprintf(v,sizeof(v),"—");
      c.drawText(mt.m,y,body,S(labels[i]));
      if(c.portrait()) {y+=body->advanceY+4;c.drawText(mt.m,y,ui,v);y+=ui->advanceY+14;}
      else {chrome::drawRight(c,mt.w-mt.m,y,ui,v);y+=body->advanceY+10;}
    }
    char line[100];
    snprintf(line,sizeof(line),U("Retained recalls: %u","Вспомнил в другие дни: %u"),progress_->confirmedCount());
    c.drawText(mt.m,y,ui,line);y+=ui->advanceY+10;
    unsigned viewed=0,practised=0;
    for(auto& s:progress_->lessons) {viewed+=s.stage>0;practised+=s.stage>=2;}
    snprintf(line,sizeof(line),U("Viewed: %u / practised: %u","Просмотрено: %u / практика: %u"),viewed,practised);
    c.drawText(mt.m,y,ui,line);y+=ui->advanceY+20;
    const char* weak[8];int n=progress_->mastery.weakTags(weak,8);
    if(n) {
      selected_%=n;strncpy(tag_,weak[selected_],sizeof(tag_)-1);
      chrome::menuRow(c,mt,y,U("PRACTISE WEAK AREA","ТРЕНИРОВАТЬ ТЕМУ"),tag_,true);
      y+=mt.menuRowH+6;
      c.drawText(mt.m,y,ui,U("Up/down: topic. OK: practise.","Вверх/вниз: тема. OK: задания."));
    } else {
      c.drawText(mt.m,y,ui,U("No weak areas with enough data.","Пока нет слабых тем с данными."));
    }
  }
  Nav handleKey(Key k) override {
    const char* weak[8];int n=progress_->mastery.weakTags(weak,8);
    if(k==Key::Back)return Nav::Done;
    if(n && (k==Key::Up || k==Key::Left))selected_=(selected_+n-1)%n;
    if(n && (k==Key::Down || k==Key::Right))selected_=(selected_+1)%n;
    if(k==Key::Ok && n){strncpy(tag_,weak[selected_%n],sizeof(tag_)-1);practice_=true;return Nav::Done;}
    return Nav::RedrawFast;
  }
 private:
  ProgressStore* progress_=nullptr;int selected_=0;bool practice_=false;char tag_[32]="";
};
