//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: help.cpp,v 1.7.2.4 2009/07/05 23:06:21 terminator356 Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <unistd.h>
#include <stdlib.h>
#include <qmessagebox.h>
#include <qprocess.h>

#include "app.h"
#include "globals.h"
#include "gconfig.h"
#include "icons.h"
#include "aboutbox.h"

//---------------------------------------------------------
//   startHelpBrowser
//---------------------------------------------------------

void MusE::startHelpBrowser()
      {
      QString lang(getenv("LANG"));
      QString museHelp = museGlobalShare + QString("/html/index_") + lang + QString(".html");
      if (access(museHelp.latin1(), R_OK) != 0) {
    	      museHelp = museGlobalShare + QString("/html/index.html");
            if (access(museHelp.latin1(), R_OK) != 0) {
                  QString info(tr("no help found at: "));
                  info += museHelp;
                  QMessageBox::critical(this, tr("MusE: Open Help"), info);
                  return;
                  }
            }
      launchBrowser(museHelp);
      }

//---------------------------------------------------------
//   startHelpBrowser
//---------------------------------------------------------

void MusE::startHomepageBrowser()
      {
      QString museHome = QString("http://www.muse-sequencer.org");

      launchBrowser(museHome);
      }

//---------------------------------------------------------
//   startBugBrowser
//---------------------------------------------------------

void MusE::startBugBrowser()
      {
      //QString museBugPage("http://www.muse-sequencer.org/wiki/index.php/Report_a_bug");
      QString museBugPage("http://www.muse-sequencer.org/index.php/Report_a_bug");
      launchBrowser(museBugPage);
      }

//---------------------------------------------------------
//   about
//---------------------------------------------------------

void MusE::about()
      {
      AboutBox ab;
      ab.show();
      ab.exec();
      }

//---------------------------------------------------------
//   aboutQt
//---------------------------------------------------------

void MusE::aboutQt()
      {
      QMessageBox::aboutQt(this, QString("MusE"));
      }

void MusE::launchBrowser(QString &whereTo)
      {
      char testStr[40];
      strcpy(testStr, "which ");
      strcat(testStr, config.helpBrowser.latin1());
      if (config.helpBrowser == "" || system(testStr))
          {
          QMessageBox::information( this, "Unable to launch help",
                                            "For some reason MusE has failed to detect or launch\n"
                                            "a browser on your machine. Please go to Settings->Global Settings->GUI\n"
                                            "and insert the program name of your favourite browser.",
                                            "Ok",
                                            0 );
          return;
          }

      QString exe = QString("/bin/sh");
      if(QFile::exists(exe))
            {
            QProcess helper(this);
            helper.addArgument(exe);
            helper.addArgument("-c");
            helper.addArgument(config.helpBrowser + " " + whereTo);
            helper.start();
            }
      else
            {
            printf("Unable to launch help\n");
            }

      }
