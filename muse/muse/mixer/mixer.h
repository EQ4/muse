//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: mixer.h,v 1.9 2006/01/12 14:49:13 wschweer Exp $
//
//  (C) Copyright 2000-2005 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __AMIXER_H__
#define __AMIXER_H__

#include "gconfig.h"

class Meter;
class Track;
class Slider;
class Knob;
class RouteDialog;
class Strip;

typedef std::list<Strip*> StripList;

//---------------------------------------------------------
//   Mixer
//---------------------------------------------------------

class Mixer : public QMainWindow {
      Q_OBJECT

      MixerConfig* cfg;
      StripList stripList;
      QWidget* central;
      QHBoxLayout* lbox;
      Strip* master;
      QHBoxLayout* layout;
      QMenu* menuView;
      RouteDialog* routingDialog;
      QAction* routingAction;

      QAction* routingId;
      QAction* showMidiTracksId;
      QAction* showMidiInPortId;
      QAction* showMidiOutPortId;
      QAction* showOutputTracksId;
      QAction* showWaveTracksId;
      QAction* showGroupTracksId;
      QAction* showInputTracksId;
      QAction* showSyntiTracksId;

      bool mustUpdateMixer;

      virtual void closeEvent(QCloseEvent*);
      void addStrip(Track*, int);
      void showRouteDialog(bool);

      enum {
            NO_UPDATE      = 0,
            STRIP_INSERTED = 1,
            STRIP_REMOVED  = 2,
            UPDATE_ALL     = 4
            };
      void updateMixer(int);

   signals:
      void closed();

   private slots:
      void songChanged(int);
      void configChanged()    { songChanged(-1); }
      void toggleRouteDialog();
      void routingDialogClosed();
      void showTracksChanged(QAction*);
      void heartBeat();

   public:
      Mixer(QWidget* parent, MixerConfig*);
      void clear();
      void write(Xml&, const char* name);
      void setUpdateMixer() { mustUpdateMixer = true; }
      };

#endif

