#include <stdio.h>
#include <qpalette.h>
#include <qlabel.h>
#include <qcolor.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QCloseEvent>
#include <QResizeEvent>
#include <values.h>

#include <qtooltip.h>
#include "globals.h"
#include "bigtime.h"
#include "song.h"
#include "app.h"
#include "gconfig.h"

extern int mtcType;

//
// the bigtime widget
// display is split into several parts to avoid flickering.
//

//---------------------------------------------------------
//   BigTime
//---------------------------------------------------------

BigTime::BigTime(QWidget* parent)
   : QWidget(parent, 0, Qt::WType_TopLevel | Qt::WStyle_StaysOnTop)
      {
      
      tickmode = true;
      dwin = new QWidget(this, "bigtime-dwin", Qt::WStyle_StaysOnTop);
      dwin->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
      fmtButton = new QCheckBox(QString(""), this);
      fmtButton->resize(18,18);
      fmtButton->setChecked(true);
      QToolTip::add(fmtButton, tr("format display"));
      barLabel   = new QLabel(dwin);
      beatLabel  = new QLabel(dwin);
      tickLabel  = new QLabel(dwin);
      //hourLabel  = new QLabel(dwin);
      minLabel   = new QLabel(dwin);
      secLabel   = new QLabel(dwin);
      frameLabel = new QLabel(dwin);
      subFrameLabel  = new QLabel(dwin);
      sep1       = new QLabel(QString("."), dwin);
      sep2       = new QLabel(QString("."), dwin);
      sep3       = new QLabel(QString(":"), dwin);
      sep4       = new QLabel(QString(":"), dwin);
      sep5       = new QLabel(QString(":"), dwin);
      absTickLabel = new QLabel(dwin);
      absFrameLabel = new QLabel(dwin);
      QToolTip::add(barLabel, tr("bar"));
      QToolTip::add(beatLabel, tr("beat"));
      QToolTip::add(tickLabel, tr("tick"));
      //QToolTip::add(hourLabel, tr("hour"));
      QToolTip::add(minLabel, tr("minute"));
      QToolTip::add(secLabel, tr("second"));
      QToolTip::add(frameLabel, tr("frame"));
      QToolTip::add(subFrameLabel, tr("subframe"));
      QToolTip::add(absTickLabel, tr("tick"));
      QToolTip::add(absFrameLabel, tr("frame"));
      fmtButtonToggled(true);
      connect(fmtButton, SIGNAL(toggled(bool)), SLOT(fmtButtonToggled(bool)));
      //oldbar = oldbeat = oldtick = oldhour = oldmin = oldsec = oldframe = -1;
      oldbar = oldbeat = oldtick = oldmin = oldsec = oldframe = oldsubframe = -1;
      oldAbsTick = oldAbsFrame = -1;
      setString(MAXINT);

      configChanged();

      QFont f(QString("Courier"));
      f.setPixelSize(10);
      
      dwin->setFont(f);
      setCaption(tr("MusE: Bigtime"));
      }


//---------------------------------------------------------
//   fmtButtonToggled
//---------------------------------------------------------

void BigTime::fmtButtonToggled(bool v)
{
    if(v)
    {
      tickmode = true;
      
      barLabel->setEnabled(true);   
      beatLabel->setEnabled(true);  
      tickLabel->setEnabled(true);
      //hourLabel->setEnabled(true);
      minLabel->setEnabled(true);
      secLabel->setEnabled(true);
      frameLabel->setEnabled(true);
      subFrameLabel->setEnabled(true);
      sep1->setEnabled(true);
      sep2->setEnabled(true);
      sep3->setEnabled(true);
      sep4->setEnabled(true);
      sep5->setEnabled(true);
      absTickLabel->setEnabled(false);
      absFrameLabel->setEnabled(false);
      
      barLabel->show();   
      beatLabel->show();  
      tickLabel->show();
      //hourLabel->show();
      minLabel->show();
      secLabel->show();
      frameLabel->show();
      subFrameLabel->show();
      sep1->show();
      sep2->show();
      sep3->show();
      sep4->show();
      sep5->show();
      absTickLabel->hide();
      absFrameLabel->hide();
    }
    else
    {
      tickmode = false;
      
      barLabel->setEnabled(false);   
      beatLabel->setEnabled(false);  
      tickLabel->setEnabled(false);
      //hourLabel->setEnabled(false);
      minLabel->setEnabled(false);
      secLabel->setEnabled(false);
      frameLabel->setEnabled(false);
      subFrameLabel->setEnabled(false);
      sep1->setEnabled(false);
      sep2->setEnabled(false);
      sep3->setEnabled(false);
      sep4->setEnabled(false);
      sep5->setEnabled(false);
      absTickLabel->setEnabled(true);
      absFrameLabel->setEnabled(true);
      
      barLabel->hide();   
      beatLabel->hide();  
      tickLabel->hide();
      //hourLabel->hide();
      minLabel->hide();
      secLabel->hide();
      frameLabel->hide();
      subFrameLabel->hide();
      sep1->hide();
      sep2->hide();
      sep3->hide();
      sep4->hide();
      sep5->hide();
      absTickLabel->show();
      absFrameLabel->show();
    }
}

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void BigTime::configChanged()
      {
      setBgColor(config.bigTimeBackgroundColor);
      setFgColor(config.bigTimeForegroundColor);
      }

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void BigTime::closeEvent(QCloseEvent *ev)
      {
      emit closed();
      QWidget::closeEvent(ev);
      }

//---------------------------------------------------------
//   setString
//---------------------------------------------------------

bool BigTime::setString(unsigned v)
      {
      if (v == MAXINT) {
            barLabel->setText(QString("----"));
            beatLabel->setText(QString("--"));
            tickLabel->setText(QString("---"));
	          //hourLabel->setText(QString("--"));
            //minLabel->setText(QString("--"));
            minLabel->setText(QString("---"));
            secLabel->setText(QString("--"));
            frameLabel->setText(QString("--"));
            subFrameLabel->setText(QString("--"));
        
        absTickLabel->setText(QString("----------"));
        absFrameLabel->setText(QString("----------"));
        oldAbsTick = oldAbsFrame = -1;
	      //oldbar = oldbeat = oldtick = oldhour = oldmin = oldsec = oldframe = -1;
        oldbar = oldbeat = oldtick = oldmin = oldsec = oldframe = oldsubframe = -1;
            return true;
            }

      unsigned absFrame = tempomap.tick2frame(v);
      int bar, beat;
      unsigned tick;
      sigmap.tickValues(v, &bar, &beat, &tick);
      double time = double(absFrame)/double(sampleRate);
      //int hour    = int(time) / 3600;
      //int min     = (int(time) / 60) % 60;
      int min     = int(time) / 60;
      int sec     = int(time) % 60;
      double rest = time - (min * 60 + sec);
      switch(mtcType) {
          case 0:     // 24 frames sec
                rest *= 24;
                break;
          case 1:     // 25
                rest *= 25;
                break;
          case 2:     // 30 drop frame
                rest *= 30;
                break;
          case 3:     // 30 non drop frame
                rest *= 30;
                break;
      }
      int frame = int(rest);
      int subframe = int((rest-frame)*100);

      QString s;

      if(oldAbsTick != v) {
        s.sprintf("%010d", v);
        absTickLabel->setText(s);
        oldAbsTick = v;
      }
      if(oldAbsFrame != absFrame) {
        s.sprintf("%010d", absFrame);
        absFrameLabel->setText(s);
        oldAbsFrame = absFrame;
      }
      if(oldbar != bar) {
	      s.sprintf("%04d", bar+1);
	      barLabel->setText(s);
	      oldbar = bar;
      }
      if(oldbeat != beat) {
	      s.sprintf("%02d", beat+1);
	      beatLabel->setText(s);
	      oldbeat = beat;
      }

      if(oldtick != tick) {
	      s.sprintf("%03d", tick);
	      tickLabel->setText(s);
	      oldtick = tick;
      }

      //if(oldhour != hour) {
	    //  s.sprintf("%02d", hour);
	    //  hourLabel->setText(s);
	    //  oldhour = hour;
      //}

      if(oldmin != min) {
	      //s.sprintf("%02d", min);
        s.sprintf("%03d", min);
	      minLabel->setText(s);
	      oldmin = min;
      }

      if(oldsec != sec) {
	      s.sprintf("%02d", sec);
	      secLabel->setText(s);
	      oldsec = sec;
      }

      if(oldframe != frame) {
	      s.sprintf("%02d", frame);
	      frameLabel->setText(s);
	      oldframe = frame;
      }

      if(oldsubframe != subframe) {
        s.sprintf("%02u", subframe);
        subFrameLabel->setText(s);
        oldsubframe = subframe;
      }

      return false;
      }

//---------------------------------------------------------
//   setPos
//---------------------------------------------------------

void BigTime::setPos(int idx, unsigned v, bool)
      {
      if (idx == 0)
            setString(v);
      }

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void BigTime::resizeEvent(QResizeEvent *ev)
      {
  dwin->resize(ev->size());    
  QFont f    = dwin->font();
      QFontMetrics fm(f);
	int fs     = f.pixelSize();
	int hspace = 20;
	//int tw     = fm.width(QString("00:00:00:00"));
  int tw     = fm.width(QString("000:00:00:00"));
  
	fs         = ((ev->size().width() - hspace*2)*fs) / tw;
// printf("resize BigTime %d -> %d, w %d\n", fs, nfs, ev->size().width());

	// set min/max
	if (fs < 10)
            fs = 10;
	else if (fs > 256)
            fs = 256;
	f.setPixelSize(fs);
	
  
  dwin->setFont(f);
  int digitWidth = dwin->fontMetrics().width(QString("0"));
	int vspace = (ev->size().height() - (fs*2)) / 3;
  int tickY = vspace;
  
	int timeY = vspace*2 + fs;
  int absTickY = tickY;
  int absFrameY = timeY;
	barLabel->resize(digitWidth*4, fs);
	beatLabel->resize(digitWidth*2, fs);
	tickLabel->resize(digitWidth*3, fs);
	//hourLabel->resize(digitWidth*2, fs);
	//minLabel->resize(digitWidth*2, fs);
  minLabel->resize(digitWidth*3, fs);
	secLabel->resize(digitWidth*2, fs);
	frameLabel->resize(digitWidth*2, fs);
  subFrameLabel->resize(digitWidth*2, fs);

  absTickLabel->resize(digitWidth*10, fs);
  absFrameLabel->resize(digitWidth*10, fs);
  sep1->resize(digitWidth, fs);
	sep2->resize(digitWidth, fs);
	sep3->resize(digitWidth, fs);
	sep4->resize(digitWidth, fs);
	sep5->resize(digitWidth, fs);
	
	barLabel->move(		hspace + (digitWidth*0), tickY);
	sep1->move(		hspace + (digitWidth*4), tickY);
	beatLabel->move(	hspace + (digitWidth*5), tickY);
	sep2->move(		hspace + (digitWidth*7), tickY);
	tickLabel->move(	hspace + (digitWidth*8), tickY);

	//hourLabel->move(	hspace + (digitWidth*0), timeY);
	//sep3->move(		hspace + (digitWidth*2), timeY);
	//minLabel->move(		hspace + (digitWidth*3), timeY);
	//sep4->move(		hspace + (digitWidth*5), timeY);
	//secLabel->move(		hspace + (digitWidth*6), timeY);
	//sep5->move(		hspace + (digitWidth*8), timeY);
	//frameLabel->move(	hspace + (digitWidth*9), timeY);
  minLabel->move(   hspace + (digitWidth*0), timeY);
  sep3->move(   hspace + (digitWidth*3), timeY);
  secLabel->move(   hspace + (digitWidth*4), timeY);
  sep4->move(   hspace + (digitWidth*6), timeY);
  frameLabel->move( hspace + (digitWidth*7), timeY);
  sep5->move(   hspace + (digitWidth*9), timeY);
  subFrameLabel->move( hspace + (digitWidth*10), timeY);
  
  absTickLabel->move( hspace + (digitWidth*0), absTickY);
  absFrameLabel->move( hspace + (digitWidth*0), absFrameY);
      }

//---------------------------------------------------------
//   setForegroundColor
//---------------------------------------------------------

void BigTime::setFgColor(QColor c)
      {
        QColorGroup cg = palette().active();
        cg.setColor(QColorGroup::Foreground, c);
        QPalette newpalette(cg, cg, cg);
        setPalette(newpalette);

        barLabel->setPalette(newpalette);
        beatLabel->setPalette(newpalette);
        tickLabel->setPalette(newpalette);
        //hourLabel->setPalette(newpalette);
        minLabel->setPalette(newpalette);
        secLabel->setPalette(newpalette);
        frameLabel->setPalette(newpalette);
        subFrameLabel->setPalette(newpalette);

        absTickLabel->setPalette(newpalette);
        absFrameLabel->setPalette(newpalette);
        sep1->setPalette(newpalette);
        sep2->setPalette(newpalette);
        sep3->setPalette(newpalette);
        sep4->setPalette(newpalette);
        sep5->setPalette(newpalette);
      }

//---------------------------------------------------------
//   setBackgroundColor
//---------------------------------------------------------

void BigTime::setBgColor(QColor c)
      {
        QColorGroup cg = palette().active();
        cg.setColor(QColorGroup::Background, c);
        QPalette newpalette(cg, cg, cg);
        setPalette(newpalette);

        barLabel->setPalette(newpalette);
        beatLabel->setPalette(newpalette);
        tickLabel->setPalette(newpalette);
        //hourLabel->setPalette(newpalette);
        minLabel->setPalette(newpalette);
        secLabel->setPalette(newpalette);
        frameLabel->setPalette(newpalette);
        subFrameLabel->setPalette(newpalette);

        absTickLabel->setPalette(newpalette);
        absFrameLabel->setPalette(newpalette);
        sep1->setPalette(newpalette);
        sep2->setPalette(newpalette);
        sep3->setPalette(newpalette);
        sep4->setPalette(newpalette);
        sep5->setPalette(newpalette);

	setBackgroundColor(c);
      }
