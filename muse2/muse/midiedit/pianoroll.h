//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: pianoroll.h,v 1.5.2.4 2009/11/16 11:29:33 lunar_shuttle Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//  (C) Copyright 2012 Tim E. Real (terminator356 on users dot sourceforge dot net)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#ifndef __PIANOROLL_H__
#define __PIANOROLL_H__

#include <QCloseEvent>
#include <QResizeEvent>
#include <QLabel>
#include <QKeyEvent>

#include <limits.h>
#include "type_defs.h"
#include "noteinfo.h"
#include "cobject.h"
#include "midieditor.h"
#include "tools.h"
#include "event.h"
#include "midictrl.h"

class QAction;
class QLabel;
class QMenu;
class QPushButton;
class QScrollArea;
class QScrollBar;
class QToolBar;
class QToolButton;
class QWidget;
class QPoint;

namespace MusECore {
class MidiPart;
class Part;
class PartList;
class Track;
class Xml;
}


namespace MusEGui {
class CtrlEdit;
class MidiTrackInfo;
class PianoCanvas;
class PitchLabel;
class SNode;
class ScrollScale;
class Splitter;
class TimeLabel;
class Toolbar1;
class Piano;

//---------------------------------------------------------
//   PianoRoll
//---------------------------------------------------------

class PianoRoll : public MidiEditor {
      Q_OBJECT
    
      QMenu *menuEdit, *menuFunctions, *menuSelect, *menuConfig, *eventColor, *menuPlugins;
      MusEGui::MidiTrackInfo *midiTrackInfo;
      MusECore::Track* selected;
      
      QAction* editCutAction; 
      QAction* editCopyAction; 
      QAction* editCopyRangeAction; 
      QAction* editPasteAction; 
      QAction* editPasteDialogAction; 
      QAction* editDelEventsAction;
      
      QAction* selectAllAction; 
      QAction* selectNoneAction;
      QAction* selectInvertAction;
      QAction* selectInsideLoopAction;
      QAction* selectOutsideLoopAction;
      QAction* selectPrevPartAction;
      QAction* selectNextPartAction;
      
      QAction* evColorBlueAction;
      QAction* evColorPitchAction;
      QAction* evColorVelAction;
      
      QAction* funcQuantizeAction;
      QAction* funcGateTimeAction;
      QAction* funcModVelAction;
      QAction* funcCrescAction;
      QAction* funcTransposeAction;
      QAction* funcEraseEventAction;
      QAction* funcNoteShiftAction;
      QAction* funcSetFixedLenAction;
      QAction* funcDelOverlapsAction;
      
      
      int tickValue;
      int lenValue;
      int pitchValue;
      int veloOnValue;
      int veloOffValue;
      bool firstValueSet;
      int tickOffset;
      int lenOffset;
      int pitchOffset;
      int veloOnOffset;
      int veloOffOffset;
      bool deltaMode;
      int lastSelections;

      MusEGui::NoteInfo* info;
      QToolButton* srec;
      QToolButton* midiin;

      Piano* piano;
      MusEGui::Toolbar1* toolbar;
      MusEGui::Splitter* splitter;
      MusEGui::Splitter* hsplitter;
      MusEGui::Splitter* ctrlLane;
      QPushButton* ctrl;
      
      QToolButton* speaker;
      QToolBar* tools;
      MusEGui::EditToolBar* tools2;

      int colorMode;

      static int _rasterInit;

      static int colorModeInit;

      bool _playEvents;

      QScrollArea* infoScroll;

      
      void initShortcuts();
      void setupNewCtrl(CtrlEdit* ctrlEdit);
      void setEventColorMode(int);
      QWidget* genToolbar(QWidget* parent);
      virtual void closeEvent(QCloseEvent*);
      virtual void keyPressEvent(QKeyEvent*);

   private slots:
      void setSelection(int /*tick*/, MusECore::Event&, MusECore::Part*, bool /*update*/);
      void noteinfoChanged(MusEGui::NoteInfo::ValType, int);
      void removeCtrl(CtrlEdit* ctrl);
      void soloChanged(bool flag);
      void setRaster(int);
      void cmd(int);
      void setSteprec(bool);
      void eventColorModeChanged(int);
      void clipboardChanged(); // enable/disable "Paste"
      void selectionChanged(); // enable/disable "Copy" & "Paste"
      void setSpeaker(bool);
      void setTime(unsigned);
      void follow(int pos);
      void songChanged1(MusECore::SongChangedFlags_t);
      void configChanged();
      void newCanvasWidth(int);
      void toggleTrackInfo();
      void updateTrackInfo();
      void deltaModeChanged(bool);
      void addCtrlClicked();
      void ctrlPopupTriggered(QAction* act);

   signals:
      void isDeleting(MusEGui::TopWin*);
   
   public slots:
      void horizontalZoom(bool zoom_in, const QPoint& glob_pos);
      void horizontalZoom(int mag, const QPoint& glob_pos);
      virtual void updateHScrollRange();
      void execDeliveredScript(int id);
      void execUserScript(int id);
      void focusCanvas();
      
   public:
      PianoRoll(MusECore::PartList*, QWidget* parent = 0, const char* name = 0, unsigned initPos = INT_MAX);
      ~PianoRoll();
      virtual void readStatus(MusECore::Xml&);
      virtual void writeStatus(int, MusECore::Xml&) const;
      static void readConfiguration(MusECore::Xml&);
      static void writeConfiguration(int, MusECore::Xml&);
      CtrlEdit* addCtrl(int ctl_num = MusECore::CTRL_VELOCITY);
      };

} // namespace MusEGui

#endif

