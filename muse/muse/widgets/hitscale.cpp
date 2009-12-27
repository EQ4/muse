//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: hitscale.cpp,v 1.3.2.1 2007/01/27 14:52:43 spamatica Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include "hitscale.h"
#include "midieditor.h"
#include <qpainter.h>

#include "song.h"

//---------------------------------------------------------
//   HitScale
//---------------------------------------------------------

HitScale::HitScale(int* r, QWidget* parent, int xs)
   : View(parent, xs, 1)
      {
      raster = r;
      pos[0] = song->cpos();
      pos[1] = song->lpos();
      pos[2] = song->rpos();
      button = QMouseEvent::NoButton;
      setMouseTracking(true);
      connect(song, SIGNAL(posChanged(int, unsigned, bool)), this, SLOT(setPos(int, unsigned, bool)));
      setFixedHeight(18);
      }

//---------------------------------------------------------
//   setPos
//---------------------------------------------------------

void HitScale::setPos(int idx, unsigned val, bool)
      {
      if (val == pos[idx])
            return;
      unsigned int opos = mapx(pos[idx]); // in order preventing comparison of sigend & unsigned int ??is this OK?
      pos[idx] = val;
      if (!isVisible())
            return;
      val = mapx(val);
      int x = -9;
      int w = 18;
      if (opos > val) {			//here would be the comparison signed/unsigned
            w += opos - val;
            x += val;
            }
      else {
            w += val - opos;
            x += opos;
            }
      paint(QRect(x, 0, w, height()));
      }

void HitScale::viewMousePressEvent(QMouseEvent* event)
      {
      button = event->button();
      viewMouseMoveEvent(event);
      }

void HitScale::viewMouseReleaseEvent(QMouseEvent*)
      {
      button = QMouseEvent::NoButton;
      }

void HitScale::viewMouseMoveEvent(QMouseEvent* event)
      {
      int x = sigmap.raster(event->x(), *raster);
      emit timeChanged(x);
      int i;
      switch (button) {
            case QMouseEvent::LeftButton:
                  i = 0;
                  break;
            case QMouseEvent::MidButton:
                  i = 1;
                  break;
            case QMouseEvent::RightButton:
                  i = 2;
                  break;
            default:
                  return;
            }
      Pos p(x, true);
      song->setPos(i, p);
      }

//---------------------------------------------------------
//   leaveEvent
//---------------------------------------------------------

void HitScale::leaveEvent(QEvent*)
      {
      emit timeChanged(-1);
      }

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void HitScale::pdraw(QPainter& p, const QRect& r)
      {
      int x = r.x();
      int w = r.width();

//      x -= 10;
//      w += 20;

      if (x < 0)
            x = 0;

      //---------------------------------------------------
      //    draw location marker
      //---------------------------------------------------

      p.setPen(red);
      int xp = mapx(pos[0]);
      if (xp >= x && xp < x+w)
            p.drawLine(xp, 0, xp, height());
      p.setPen(blue);
      xp = mapx(pos[1]);
      if (xp >= x && xp < x+w)
            p.drawLine(xp, 0, xp, height());
      xp = mapx(pos[2]);
      if (xp >= x && xp < x+w)
            p.drawLine(xp, 0, xp, height());
      }


