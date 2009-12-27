//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: sync.cpp,v 1.6.2.12 2009/06/20 22:20:41 terminator356 Exp $
//
//  (C) Copyright 2003 Werner Schweer (ws@seh.de)
//=========================================================

#include <cmath>
#include "sync.h"
#include "song.h"
#include "utils.h"
#include "midiport.h"
#include "mididev.h"
#include "globals.h"
#include "midiseq.h"
#include "audio.h"
#include "audiodev.h"
#include "gconfig.h"
#include "xml.h"

//int rxSyncPort = -1;         // receive from all ports
//int txSyncPort = 1;
//int rxDeviceId = 0x7f;       // any device
//int txDeviceId = 0x7f;       // any device
//MidiSyncPort midiSyncPorts[MIDI_PORTS];
int curMidiSyncInPort = -1;

bool debugSync = false;
int mtcType     = 1;
MTC mtcOffset;
BValue extSyncFlag(0, "extSync");       // false - MASTER, true - SLAVE
//bool genMTCSync = false;      // output MTC Sync
//bool genMCSync  = false;      // output MidiClock Sync
//bool genMMC     = false;      // output Midi Machine Control
//bool acceptMTC  = false;
//bool acceptMC   = true;
//bool acceptMMC  = true;

static MTC mtcCurTime;
static int mtcState;    // 0-7 next expected quarter message
static bool mtcValid;
static int mtcLost;
static bool mtcSync;    // receive complete mtc frame?

// Not used yet.
// static bool mcStart = false;
// static int mcStartTick;

//---------------------------------------------------------
//  MidiSyncInfo
//---------------------------------------------------------

MidiSyncInfo::MidiSyncInfo()
{
  _port          = -1;
  _idOut         = 127;
  _idIn          = 127;
  _sendMC        = false;
  _sendMMC       = false;
  _sendMTC       = false;
  _recMC         = false;
  _recMMC        = false;
  _recMTC        = false;
  
  _lastClkTime   = 0.0;
  _lastTickTime  = 0.0;
  _clockTrig     = false;
  _tickTrig      = false;
  _clockDetect   = false;
  _tickDetect    = false;
  _actDetectBits = 0;
  for(int i = 0; i < MIDI_CHANNELS; ++i)
  {
    _lastActTime[i] = 0.0;
    _actTrig[i]     = false;
    _actDetect[i]   = false;
  }
}

//---------------------------------------------------------
//   operator =
//---------------------------------------------------------

MidiSyncInfo& MidiSyncInfo::operator=(const MidiSyncInfo &sp)
{
  //_port          = sp._port;
  
  copyParams(sp);
  
  _lastClkTime   = sp._lastClkTime;
  _lastTickTime  = sp._lastTickTime;
  _clockTrig     = sp._clockTrig;
  _tickTrig      = sp._tickTrig;
  _clockDetect   = sp._clockDetect;
  _tickDetect    = sp._tickDetect;
  _actDetectBits = sp._actDetectBits;
  for(int i = 0; i < MIDI_CHANNELS; ++i)
  {
    _lastActTime[i] = sp._lastActTime[i];
    _actTrig[i]     = sp._actTrig[i];
    _actDetect[i]   = sp._actDetect[i];
  }
  return *this;
}

//---------------------------------------------------------
//   copyParams
//---------------------------------------------------------

MidiSyncInfo& MidiSyncInfo::copyParams(const MidiSyncInfo &sp)
{
  //_port          = sp._port;
  
  _idOut         = sp._idOut;
  _idIn          = sp._idIn;
  _sendMC        = sp._sendMC;
  _sendMMC       = sp._sendMMC;
  _sendMTC       = sp._sendMTC;
  setMCIn(sp._recMC);
  _recMMC        = sp._recMMC;
  _recMTC        = sp._recMTC;
  return *this;
}

//---------------------------------------------------------
//  setTime
//---------------------------------------------------------

void MidiSyncInfo::setTime() 
{ 
  // Note: CurTime() makes a system call to gettimeofday(), 
  //  which apparently can be slow in some cases. So I avoid calling this function
  //  too frequently by calling it (at the heartbeat rate) in Song::beat().  T356
  double t = curTime();
  
  if(_clockTrig)
  {
    _clockTrig = false;
    _lastClkTime = t;  
  }
  else
  if(_clockDetect && (t - _lastClkTime >= 1.0)) // Set detect indicator timeout to about 1 second.
  {
    _clockDetect = false;
    if(curMidiSyncInPort == _port)
      curMidiSyncInPort = -1;
  }
  
  if(_tickTrig)
  {
    _tickTrig = false;
    _lastTickTime = t;  
  }
  else
  if(_tickDetect && (t - _lastTickTime) >= 1.0) // Set detect indicator timeout to about 1 second.
    _tickDetect = false;
    
  for(int i = 0; i < MIDI_CHANNELS; i++)
  {
    if(_actTrig[i])
    {
      _actTrig[i] = false;
      _lastActTime[i] = t;  
    }
    else
    if(_actDetect[i] && (t - _lastActTime[i]) >= 1.0) // Set detect indicator timeout to about 1 second.
    {
      _actDetect[i] = false;
      _actDetectBits &= ~bitShiftLU[i];
    }  
  }
}

//---------------------------------------------------------
//  setMCIn
//---------------------------------------------------------

void MidiSyncInfo::setMCIn(const bool v) 
{ 
  _recMC = v; 
  // If sync receive was turned off, clear the current midi sync in port number so another port can grab it.
  if(!_recMC && _port != -1 && curMidiSyncInPort == _port)
    curMidiSyncInPort = -1;
}

//---------------------------------------------------------
//  trigMCSyncDetect
//---------------------------------------------------------

void MidiSyncInfo::trigMCSyncDetect() 
{ 
  _clockDetect = true;
  _clockTrig = true;
  // Set the current midi sync in port number if it's not taken...
  if(_recMC && curMidiSyncInPort == -1)
    curMidiSyncInPort = _port;
}

//---------------------------------------------------------
//  trigTickDetect
//---------------------------------------------------------

void MidiSyncInfo::trigTickDetect()   
{ 
  _tickDetect = true;
  _tickTrig = true;
}
    
//---------------------------------------------------------
//  actDetect
//---------------------------------------------------------

bool MidiSyncInfo::actDetect(const int ch) const
{ 
  if(ch < 0 || ch >= MIDI_CHANNELS)
    return false;
    
  return _actDetect[ch]; 
}           

//---------------------------------------------------------
//  trigActDetect
//---------------------------------------------------------

void MidiSyncInfo::trigActDetect(const int ch)   
{ 
  if(ch < 0 || ch >= MIDI_CHANNELS)
    return;
    
  _actDetectBits |= bitShiftLU[ch];
  _actDetect[ch] = true;
  _actTrig[ch] = true;
}
    
//---------------------------------------------------------
//   read
//---------------------------------------------------------

void MidiSyncInfo::read(Xml& xml)
      {
      for (;;) {
            Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case Xml::Error:
                  case Xml::End:
                        return;
                  case Xml::TagStart:
                             if (tag == "idOut")
                              _idOut = xml.parseInt();
                        else if (tag == "idIn")
                              _idIn = xml.parseInt();
                        else if (tag == "sendMC")
                              _sendMC = xml.parseInt();
                        else if (tag == "sendMMC")
                              _sendMMC = xml.parseInt();
                        else if (tag == "sendMTC")
                              _sendMTC = xml.parseInt();
                        else if (tag == "recMC")
                              _recMC = xml.parseInt();
                        else if (tag == "recMMC")
                              _recMMC = xml.parseInt();
                        else if (tag == "recMTC")
                              _recMTC = xml.parseInt();
                        else
                              xml.unknown("midiSyncInfo");
                        break;
                  case Xml::TagEnd:
                        if (tag == "midiSyncInfo")
                            return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//  write
//---------------------------------------------------------

//void MidiSyncInfo::write(int level, Xml& xml, MidiDevice* md)
void MidiSyncInfo::write(int level, Xml& xml)
{
  //if(!md)
  //  return;
  
  // All defaults? Nothing to write.
  if(_idOut == 127 && _idIn == 127 && !_sendMC && !_sendMMC && !_sendMTC && !_recMC && !_recMMC && !_recMTC)
    return;
  
  xml.tag(level++, "midiSyncInfo");
  //xml.intTag(level, "idx", idx);
  //xml.intTag(level++, "midiSyncPort", idx);
  //xml.tag(level++, "midiSyncInfo idx=\"%d\"", idx);
  
  //xml.strTag(level, "device", md->name());
  
  if(_idOut != 127)
    xml.intTag(level, "idOut", _idOut);
  if(_idIn != 127)
    xml.intTag(level, "idIn", _idIn);
  
  if(_sendMC)
    xml.intTag(level, "sendMC", true);
  if(_sendMMC)
    xml.intTag(level, "sendMMC", true);
  if(_sendMTC)
    xml.intTag(level, "sendMTC", true);
  
  if(_recMC)
    xml.intTag(level, "recMC", true);
  if(_recMMC)
    xml.intTag(level, "recMMC", true);
  if(_recMTC)
    xml.intTag(level, "recMTC", true);
  
  xml.etag(level, "midiSyncInfo");
}

//---------------------------------------------------------
//  mmcInput
//    Midi Machine Control Input received
//---------------------------------------------------------

void MidiSeq::mmcInput(const unsigned char* p, int n)
      {
      if (debugSync)
            printf("mmcInput: n:%d %02x %02x %02x %02x\n",
               n, p[2], p[3], p[4], p[5]);
      //if (!(extSyncFlag.value() && acceptMMC))
      if(!extSyncFlag.value())
            return;

      switch(p[3]) {
            case 1:
                  if (debugSync)
                        printf("  MMC: STOP\n");
                  //if ((state == PLAY || state == PRECOUNT))
                  if (audio->isPlaying())
                        audio->msgPlay(false);
                        playStateExt = false;
                        alignAllTicks();
                        //stopPlay();
                  break;
            case 2:
                  if (debugSync)
                        printf("  MMC: PLAY\n");
            case 3:
                  if (debugSync)
                        printf("  MMC: DEFERRED PLAY\n");
                  mtcState = 0;
                  mtcValid = false;
                  mtcLost  = 0;
                  mtcSync  = false;
                  //startPlay();
                  alignAllTicks();
                  audio->msgPlay(true);
                  playStateExt = true;
                  break;

            case 4:
                  printf("MMC: FF not implemented\n");
                  break;
            case 5:
                  printf("MMC: REWIND not implemented\n");
                  break;
            case 6:
                  printf("MMC: REC STROBE not implemented\n");
                  break;
            case 7:
                  printf("MMC: REC EXIT not implemented\n");
                  break;
            case 0xd:
                  printf("MMC: RESET not implemented\n");
                  break;
            case 0x44:
                  if (p[5] == 0) {
                        printf("MMC: LOCATE IF not implemented\n");
                        break;
                        }
                  else if (p[5] == 1) {
                        MTC mtc(p[6] & 0x1f, p[7], p[8], p[9], p[10]);
                        int mmcPos = tempomap.frame2tick(lrint(mtc.time()*sampleRate));

                        Pos tp(mmcPos, true);
                        if (!checkAudioDevice()) return;
                        //audioDevice->seekTransport(tp.frame());
                        audioDevice->seekTransport(tp);
                        alignAllTicks();
                        //seek(tp);
                        if (debugSync) {
                              printf("MMC: %f %d seek ",
                                 mtc.time(), mmcPos);
                              mtc.print();
                              printf("\n");
                              }
                        //write(sigFd, "G", 1);
                        break;
                        }
                  // fall through
            default:
                  printf("MMC %x %x, unknown\n", p[3], p[4]); break;
            }
      }

//---------------------------------------------------------
//   mtcInputQuarter
//    process Quarter Frame Message
//---------------------------------------------------------

void MidiSeq::mtcInputQuarter(int, unsigned char c)
      {
      static int hour, min, sec, frame;

      if (!extSyncFlag.value())
            return;

      int valL = c & 0xf;
      int valH = valL << 4;

      int _state = (c & 0x70) >> 4;
      if (mtcState != _state)
            mtcLost += _state - mtcState;
      mtcState = _state + 1;

      switch(_state) {
            case 7:
                  hour  = (hour  & 0x0f) | valH;
                  break;
            case 6:
                  hour  = (hour  & 0xf0) | valL;
                  break;
            case 5:
                  min   = (min   & 0x0f) | valH;
                  break;
            case 4:
                  min   = (min   & 0xf0) | valL;
                  break;
            case 3:
                  sec   = (sec   & 0x0f) | valH;
                  break;
            case 2:
                  sec   = (sec   & 0xf0) | valL;
                  break;
            case 1:
                  frame = (frame & 0x0f) | valH;
                  break;
            case 0:  frame = (frame & 0xf0) | valL;
                  break;
            }
      frame &= 0x1f;    // 0-29
      sec   &= 0x3f;    // 0-59
      min   &= 0x3f;    // 0-59
      hour  &= 0x1f;

      if (mtcState == 8) {
            mtcValid = (mtcLost == 0);
            mtcState = 0;
            mtcLost  = 0;
            if (mtcValid) {
                  mtcCurTime.set(hour, min, sec, frame);
                  mtcSyncMsg(mtcCurTime, !mtcSync);
                  mtcSync = true;
                  }
            }
      else if (mtcValid && (mtcLost == 0)) {
            mtcCurTime.incQuarter();
            mtcSyncMsg(mtcCurTime, false);
            }
      }

//---------------------------------------------------------
//   mtcInputFull
//    process Frame Message
//---------------------------------------------------------

void MidiSeq::mtcInputFull(const unsigned char* p, int n)
      {
      if (debugSync)
            printf("mtcInputFull\n");
      if (!extSyncFlag.value())
            return;

      if (p[3] != 1) {
            if (p[3] != 2) {   // silently ignore user bits
                  printf("unknown mtc msg subtype 0x%02x\n", p[3]);
                  dump(p, n);
                  }
            return;
            }
      int hour  = p[4];
      int min   = p[5];
      int sec   = p[6];
      int frame = p[7];

      frame &= 0x1f;    // 0-29
      sec   &= 0x3f;    // 0-59
      min   &= 0x3f;    // 0-59
//      int type = (hour >> 5) & 3;
      hour &= 0x1f;

      mtcCurTime.set(hour, min, sec, frame);
      mtcState = 0;
      mtcValid = true;
      mtcLost  = 0;
      }

//---------------------------------------------------------
//   nonRealtimeSystemSysex
//---------------------------------------------------------

void MidiSeq::nonRealtimeSystemSysex(const unsigned char* p, int n)
      {
//      int chan = p[2];
      switch(p[3]) {
            case 4:
                  printf("NRT Setup\n");
                  break;
            default:
                  printf("unknown NRT Msg 0x%02x\n", p[3]);
                  dump(p, n);
                  break;
           }
      }

//---------------------------------------------------------
//   setSongPosition
//    MidiBeat is a 14 Bit value. Each MidiBeat spans
//    6 MIDI Clocks. Inother words, each MIDI Beat is a
//    16th note (since there are 24 MIDI Clocks in a
//    quarter note).
//---------------------------------------------------------

void MidiSeq::setSongPosition(int port, int midiBeat)
      {
      if (midiInputTrace)
            printf("set song position port:%d %d\n", port, midiBeat);
      
      midiPorts[port].syncInfo().trigMCSyncDetect();
      
      //if (!extSyncFlag.value())
      // External sync not on? Clock in not turned on? 
      if(!extSyncFlag.value() || !midiPorts[port].syncInfo().MCIn())
            return;
            
      Pos pos((config.division * midiBeat) / 4, true);
      
      // Re-transmit song position to other devices if clock out turned on.
      for(int p = 0; p < MIDI_PORTS; ++p)
        if(p != port && midiPorts[p].syncInfo().MCOut())
          midiPorts[p].sendSongpos(midiBeat);
                  
      if (!checkAudioDevice()) return;

      //audioDevice->seekTransport(pos.frame());
      audioDevice->seekTransport(pos);
      alignAllTicks(pos.frame());
      if (debugSync)
            printf("setSongPosition %d\n", pos.tick());
      }



//---------------------------------------------------------
//   set all runtime variables to the "in sync" value
//---------------------------------------------------------
void MidiSeq::alignAllTicks(int frameOverride)
      {
      //printf("alignAllTicks audioDriver->framePos=%d, audio->pos().frame()=%d\n", 
      //        audioDevice->framePos(), audio->pos().frame());
      unsigned curFrame;
      if (!frameOverride)
        curFrame = audio->pos().frame();
      else
        curFrame = frameOverride;

      int tempo = tempomap.tempo(0);

      // use the last old values to give start values for the tripple buffering
      int recTickSpan = recTick1 - recTick2;
      int songTickSpan = (int)(songtick1 - songtick2);    //prevent compiler warning:  casting double to int
      storedtimediffs = 0; // pretend there is no sync history

      mclock2=mclock1=0.0; // set all clock values to "in sync"

      recTick = (int) ((double(curFrame)/double(sampleRate)) *
                        double(config.division * 1000000.0) / double(tempo) //prevent compiler warning:  casting double to int
                );
      songtick1 = recTick - songTickSpan;
      if (songtick1 < 0)
        songtick1 = 0;
      songtick2 = songtick1 - songTickSpan;
      if (songtick2 < 0)
        songtick2 = 0;
      recTick1 = recTick - recTickSpan;
      if (recTick1 < 0)
        recTick1 = 0;
      recTick2 = recTick1 - recTickSpan;
      if (recTick2 < 0)
        recTick2 = 0;
      if (debugSync)
        printf("alignAllTicks curFrame=%d recTick=%d tempo=%.3f frameOverride=%d\n",curFrame,recTick,(float)((1000000.0 * 60.0)/tempo), frameOverride);

      }

//---------------------------------------------------------
//   realtimeSystemInput
//    real time message received
//---------------------------------------------------------
void MidiSeq::realtimeSystemInput(int port, int c)
      {

      if (midiInputTrace)
            printf("realtimeSystemInput port:%d 0x%x\n", port+1, c);

      //if (midiInputTrace && (rxSyncPort != port) && rxSyncPort != -1) {
      //      if (debugSync)
      //            printf("rxSyncPort configured as %d; received sync from port %d\n",
      //               rxSyncPort, port);
      //      return;
      //      }
      
      MidiPort* mp = &midiPorts[port];
      
      // Trigger on any tick, clock, or realtime command. 
      if(c == 0xf9)
        mp->syncInfo().trigTickDetect();
      else
        mp->syncInfo().trigMCSyncDetect();
       
      // External sync not on? Clock in not turned on? 
      if(!extSyncFlag.value() || !mp->syncInfo().MCIn())
        return;
      
      switch(c) {
            case 0xf8:  // midi clock (24 ticks / quarter note)
                  {
                  // Not for the current in port? Forget it.
                  if(port != curMidiSyncInPort)
                    break;
                  
                  // Re-transmit clock to other devices if clock out turned on.
                  // Must be careful not to allow more than one clock input at a time.
                  // Would re-transmit mixture of multiple clocks - confusing receivers.
                  // Solution: Added curMidiSyncInPort. 
                  // Maybe in MidiSeq::processTimerTick(), call sendClock for the other devices, instead of here.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    if(p != port && midiPorts[p].syncInfo().MCOut())
                      midiPorts[p].sendClock();
                  
                  double mclock0 = curTime();
                  // Difference in time last 2 rounds:
                  double tdiff0   = mclock0 - mclock1;
                  double tdiff1   = mclock1 - mclock2;
                  double averagetimediff = 0.0;

                  if (mclock1 != 0.0) {
                        if (storedtimediffs < 24)
                        {
                           timediff[storedtimediffs] = mclock0 - mclock1;
                           storedtimediffs++;
                        }
                        else {
                              for (int i=0; i<23; i++) {
                                    timediff[i] = timediff[i+1];
                                    }
                              timediff[23] = mclock0 - mclock1;
                        }
                        // Calculate average timediff:
                        for (int i=0; i < storedtimediffs; i++) {
                              averagetimediff += timediff[i]/storedtimediffs;
                              }
                        }

                  // Compare w audio if playing:
                  if (playStateExt == true /* audio->isPlaying() */ /*state == PLAY*/) {
                        //BEGIN standard setup:
                        recTick  += config.division / 24; // The one we're syncing to
                        int tempo = tempomap.tempo(0);
                        unsigned curFrame = audio->pos().frame();
                        double songtick = (double(curFrame)/double(sampleRate)) *
                                           double(config.division * 1000000.0) / double(tempo);

                        double scale = double(tdiff0/averagetimediff);
                        double tickdiff = songtick - ((double) recTick - 24 + scale*24.0);

                        //END standard setup
                        if (debugSync) {
                              int m, b, t;
                              audio->pos().mbt(&m, &b, &t);

                              int song_beat = b + m*4; // if the time-signature is different than 4/4, this will be wrong.
                              int sync_beat = recTick/config.division;
                              printf("pT=%.3f rT=%d diff=%.3f songB=%d syncB=%d scale=%.3f, curFrame=%d", 
                                      songtick, recTick, tickdiff, song_beat, sync_beat, scale, curFrame);
                              }

                        if ((mclock2 !=0.0) && (tdiff1 > 0.0) /*&& fabs(tickdiff) > 0.5*/ && lastTempo != 0) {
                              // Interpolate:
                              double tickdiff1 = songtick1 - recTick1;
                              double tickdiff2 = songtick2 - recTick2;
                              double newtickdiff = (tickdiff1+tickdiff2)/250; /*tickdiff/5.0  +
                                                   tickdiff1/16.0 +
                                                   tickdiff2/24.0;*/  //5 mins 30 secs on 116BPM, -p 512 jackd

                              if (newtickdiff != 0.0) {
                                    int newTempo = tempomap.tempo(0);
                                    //newTempo += int(24.0 * newtickdiff * scale);
                                    newTempo += int(24.0 * newtickdiff);
                                    if (debugSync)
                                          printf(" tdiff=%f ntd=%f lt=%d tmpo=%.3f", 
                                                tdiff0, newtickdiff, lastTempo, (float)((1000000.0 * 60.0)/newTempo));
                                    //syncTempo = newTempo;
                                    tempomap.setTempo(0,newTempo);
                                    }
                              if (debugSync)
                                    printf("\n");
                              }
                        else if (debugSync)
                              printf("\n");

                        //BEGIN post calc
                        lastTempo = tempo;
                        recTick2 = recTick1;
                        recTick1 = recTick;
                        mclock2 = mclock1;
                        mclock1 = mclock0;
                        songtick2 = songtick1;
                        songtick1 = songtick;
                        //END post calc
                        break;
                        } // END state play
                  //
                  // Pre-sync (when audio is not running)
                  // Calculate tempo depending on time per pulse
                  //
                  if (mclock1 == 0.0) {
                        mp->device()->discardInput();
                        if (debugSync)
                           printf("Discarding input from port %d\n", port);
                        }
                  if ((mclock2 != 0.0) && (tdiff0 > 0.0)) {
                        int tempo0 = int(24000000.0 * tdiff0 + .5);
                        int tempo1 = int(24000000.0 * tdiff1 + .5);
                        int tempo = tempomap.tempo(0);

                        int diff0 = tempo0 - tempo;
                        int diff1 = tempo1 - tempo0;
                        if (diff0) {
                              int newTempo = tempo + diff0/8 + diff1/16;
                              if (debugSync)
                                 printf("setting new tempo %d = %f\n", newTempo, (float)((1000000.0 * 60.0)/newTempo));
                              tempomap.setTempo(0, newTempo);
                              }
                        }
                  mclock2 = mclock1;
                  mclock1 = mclock0;
                  }
                  break;
            case 0xf9:  // midi tick  (every 10 msec)
                  // FIXME: Unfinished? mcStartTick is uninitialized and Song::setPos doesn't set it either. Dangerous to allow this.
                  //if (mcStart) {
                  //      song->setPos(0, mcStartTick);
                  //      mcStart = false;
                  //      return;
                  //      }
                  break;
            case 0xfa:  // start
                  // Re-transmit start to other devices if clock out turned on.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    if(p != port && midiPorts[p].syncInfo().MCOut())
                      midiPorts[p].sendStart();
                  
                  if (debugSync)
                        printf("   start\n");
                  if (1 /* !audio->isPlaying()*/ /*state == IDLE*/) {
                        if (!checkAudioDevice()) return;
                        //audioDevice->seekTransport(0);
                        audioDevice->seekTransport(Pos(0, false));
                        unsigned curFrame = audio->curFrame();
                        if (debugSync)
                              printf("       curFrame=%d\n", curFrame);
                        
                        alignAllTicks();
                        if (debugSync)
                              printf("   curFrame: %d curTick: %d tempo: %d\n", curFrame, recTick, tempomap.tempo(0));

                        storedtimediffs = 0;
                        for (int i=0; i<24; i++)
                              timediff[i] = 0.0;
                        audio->msgPlay(true);
                        playStateExt = true;
                        }
                  break;
            case 0xfb:  // continue
                  // Re-transmit continue to other devices if clock out turned on.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    if(p != port && midiPorts[p].syncInfo().MCOut())
                      midiPorts[p].sendContinue();
                  
                  if (debugSync)
                        printf("   continue\n");
                  if (1 /* !audio->isPlaying() */ /*state == IDLE */) {
                        //unsigned curFrame = audio->curFrame();
                        //recTick = tempomap.frame2tick(curFrame); // don't think this will work... (ml)
                        //alignAllTicks();
                        audio->msgPlay(true);
                        playStateExt = true;
                        }
                  break;
            case 0xfc:  // stop
                  // Re-transmit stop to other devices if clock out turned on.
                  for(int p = 0; p < MIDI_PORTS; ++p)
                    if(p != port && midiPorts[p].syncInfo().MCOut())
                      midiPorts[p].sendStop();
                  
                  if (debugSync)
                        printf("   stop\n");
                  if (audio->isPlaying() /*state == PLAY*/) {
                        audio->msgPlay(false);
                        playStateExt = false;
                        }
                  break;
            case 0xfd:  // unknown
            case 0xfe:  // active sensing
            case 0xff:  // system reset
                  break;
            }

      }

//---------------------------------------------------------
//   mtcSyncMsg
//    process received mtc Sync
//    seekFlag - first complete mtc frame received after
//                start
//---------------------------------------------------------

void MidiSeq::mtcSyncMsg(const MTC& mtc, bool seekFlag)
      {
      double time = mtc.time();
      if (debugSync)
            printf("mtcSyncMsg: time %f\n", time);

      if (seekFlag && audio->isRunning() /*state == START_PLAY*/) {
//            int tick = tempomap.time2tick(time);
            //state = PLAY;
            //write(sigFd, "1", 1);  // say PLAY to gui
            if (!checkAudioDevice()) return;
            audioDevice->startTransport();
            return;
            }

      /*if (tempoSN != tempomap.tempoSN()) {
            double cpos    = tempomap.tick2time(_midiTick, 0);
            samplePosStart = samplePos - lrint(cpos * sampleRate);
            rtcTickStart   = rtcTick - lrint(cpos * realRtcTicks);
            tempoSN        = tempomap.tempoSN();
            }*/

      //
      // diff is the time in sec MusE is out of sync
      //
      /*double diff = time - (double(samplePosStart)/double(sampleRate));
      if (debugSync)
            printf("   state %d diff %f\n", mtcState, diff);
      */
      }


