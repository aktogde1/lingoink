#pragma once
#include "Screen.h"
#include "../ui/Chrome.h"
#include "../ui/Strings.h"
#include "../progress/ProgressStore.h"

class DateScreen: public Screen {
 public:
  void start(ProgressStore* p) {
    progress_=p; field_=2; error_=false; accepted_=false;
    uint32_t date=p->calendarDate;
    if(!date) {
      char mon[4]; int day,year; sscanf(__DATE__,"%3s %d %d",mon,&day,&year);
      const char* names="JanFebMarAprMayJunJulAugSepOctNovDec";
      const char* at=strstr(names,mon); int month=at?((at-names)/3+1):1;
      date=year*10000+month*100+day;
    }
    y_=date/10000; m_=(date/100)%100; d_=date%100;
  }
  bool accepted() const {return accepted_;}
  void render(Canvas& c) override {
    auto mt=chrome::metrics(c); auto body=fontByRole(FontRole::Body);
    chrome::header(c,mt,U("TODAY'S DATE","ДАТА СЕГОДНЯ"));
    const char* labels[]={U("Year","Год"),U("Month","Месяц"),U("Day","День")};
    int vals[]={y_,m_,d_};
    for(int i=0;i<3;++i) {char v[16];snprintf(v,sizeof(v),"%d",vals[i]); chrome::settingRow(c,mt,mt.top+i*(mt.rowH+8),labels[i],v,field_==i);}
    int y=mt.top+3*(mt.rowH+8)+16;
    const char* text=error_?U("Date cannot go backwards.","Дата не может идти назад."):
      U("Confirm the actual date for reviews. Left/right: field. Up/down: value. OK: save. Back: cancel.",
        "Укажите текущую дату для повторов. Влево/вправо: поле. Вверх/вниз: значение. OK: сохранить. Назад: отмена.");
    const char* lines[12]; int len[12]; int n=c.wrapText(body,text,mt.w-2*mt.m,lines,len,12);
    for(int i=0;i<n && y+body->advanceY<=mt.bottom;++i,y+=body->advanceY+4) c.drawTextN(mt.m,y,body,lines[i],len[i]);
  }
  Nav handleKey(Key k) override {
    accepted_=false;
    if(k==Key::Back) return Nav::Done;
    if(k==Key::Left) field_=(field_+2)%3;
    if(k==Key::Right) field_=(field_+1)%3;
    int delta=k==Key::Up?1:(k==Key::Down?-1:0);
    if(delta) {
      if(field_==0) {y_+=delta; if(y_<2020)y_=2020; if(y_>2099)y_=2099;}
      if(field_==1) {m_+=delta;if(m_<1)m_=12;if(m_>12)m_=1;}
      if(field_==2) {d_+=delta;if(d_<1)d_=monthDays(y_,m_);if(d_>monthDays(y_,m_))d_=1;}
      if(d_>monthDays(y_,m_))d_=monthDays(y_,m_);
    }
    if(k==Key::Ok) {
      if(progress_->setDate(y_*10000+m_*100+d_)) {accepted_=true; return Nav::Done;}
      error_=true;
    }
    return Nav::RedrawFull;
  }
 private:
  ProgressStore* progress_=nullptr;
  int y_=2026,m_=1,d_=1,field_=2; bool error_=false,accepted_=false;
};
