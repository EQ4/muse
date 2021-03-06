//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: node.cpp,v 1.36.2.25 2009/12/20 05:00:35 terminator356 Exp $
//
//  (C) Copyright 2000-2004 Werner Schweer (ws@seh.de)
//  (C) Copyright 2011-2013 Tim E. Real (terminator356 on sourceforge)
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

#include <cmath>
#include <sndfile.h>
#include <stdlib.h>
#include <stdio.h>

#include <QString>

#include "node.h"
#include "globals.h"
#include "gconfig.h"
#include "song.h"
#include "xml.h"
#include "plugin.h"
#include "synth.h"
#include "audiodev.h"
#include "audio.h"
#include "wave.h"
#include "utils.h"      //debug
#include "ticksynth.h"  // metronome
#include "wavepreview.h"
#include "al/dsp.h"

// Turn on debugging messages
//#define NODE_DEBUG     
// Turn on constant flow of process debugging messages
//#define NODE_DEBUG_PROCESS     

//#define FIFO_DEBUG 
//#define METRONOME_DEBUG 

namespace MusECore {

//---------------------------------------------------------
//   isMute
//---------------------------------------------------------

bool MidiTrack::isMute() const
      {
      if (_solo || (_internalSolo && !_mute))
            return false;
      
      if (_soloRefCnt)
            return true;
      
      return _mute;
      }

bool AudioTrack::isMute() const
      {
      if (_solo || (_internalSolo && !_mute))
            return false;
      
      if (_soloRefCnt)
            return true;
      
      return _mute;
      }


//---------------------------------------------------------
//   setSolo
//---------------------------------------------------------

void MidiTrack::setSolo(bool val)
{
      if(_solo != val)
      {
        _solo = val;
        updateSoloStates(false);
      }
}

void AudioTrack::setSolo(bool val)
{
      if(_solo != val)
      {
        _solo = val;
        updateSoloStates(false);
      }
      
      if (isMute())
            resetMeter();
}


//---------------------------------------------------------
//   setInternalSolo
//---------------------------------------------------------

void Track::setInternalSolo(unsigned int val)
{
  _internalSolo = val;
}

//---------------------------------------------------------
//   clearSoloRefCounts
//   This is a static member function. Required for outside access.
//   Clears the internal static reference counts. 
//---------------------------------------------------------

void Track::clearSoloRefCounts()
{
  _soloRefCnt = 0;
}

//---------------------------------------------------------
//   updateSoloState
//---------------------------------------------------------

void Track::updateSoloState()
{
    if(_solo)
      _soloRefCnt++;
    else
    if(_soloRefCnt && !_tmpSoloChainNoDec)
      _soloRefCnt--;
}

//---------------------------------------------------------
//   updateInternalSoloStates
//---------------------------------------------------------

void Track::updateInternalSoloStates()
{
    if(_tmpSoloChainTrack->solo())
    {
      _internalSolo++;
      _soloRefCnt++;
    }  
    else
    if(!_tmpSoloChainNoDec) 
    {                           
      if(_internalSolo)
        _internalSolo--;
      if(_soloRefCnt)
        _soloRefCnt--;
    }  
}

//---------------------------------------------------------
//   updateInternalSoloStates
//---------------------------------------------------------

void MidiTrack::updateInternalSoloStates()
{
  if(_nodeTraversed)         // Anti circular mechanism.
  {
    fprintf(stderr, "MidiTrack::updateInternalSoloStates %s :\n  MusE Warning: Please check your routes: Circular path found!\n", name().toLatin1().constData()); 
    return;                         
  }  
  //if(this == _tmpSoloChainTrack)  
  //  return;
  
  _nodeTraversed = true; 
  
  Track::updateInternalSoloStates();
  
  _nodeTraversed = false;  // Reset.
}


//---------------------------------------------------------
//   updateInternalSoloStates
//---------------------------------------------------------

void AudioTrack::updateInternalSoloStates()
{
  if(_nodeTraversed)         // Anti circular mechanism.
  {
    fprintf(stderr, "AudioTrack::updateInternalSoloStates %s :\n  MusE Warning: Please check your routes: Circular path found!\n", name().toLatin1().constData()); 
    return;                         
  }  
  //if(this == _tmpSoloChainTrack)  
  //  return;
  
  _nodeTraversed = true; 

  Track::updateInternalSoloStates();
  
  if(_tmpSoloChainDoIns)
  {
    if(type() == AUDIO_SOFTSYNTH)
    {
      const MusECore::MidiTrackList* ml = MusEGlobal::song->midis();
      for(MusECore::ciMidiTrack im = ml->begin(); im != ml->end(); ++im)
      {
        MusECore::MidiTrack* mt = *im;
        if(mt->outPort() >= 0 && mt->outPort() == ((SynthI*)this)->midiPort())
          mt->updateInternalSoloStates();
      }
    }
    
    const RouteList* rl = inRoutes();
    for(ciRoute ir = rl->begin(); ir != rl->end(); ++ir)
    {
      if(ir->type == Route::TRACK_ROUTE)
        ir->track->updateInternalSoloStates();
      else  
      // Support Midi Port -> Audio Input solo chains. p4.0.37 Tim.
      if(ir->type == Route::MIDI_PORT_ROUTE)    
      {
        const MidiTrackList* ml = MusEGlobal::song->midis();
        for(ciMidiTrack im = ml->begin(); im != ml->end(); ++im)
        {
          MidiTrack* mt = *im;
          if(mt->outPort() == ir->midiPort && ((1 << mt->outChannel()) & ir->channel) )
            mt->updateInternalSoloStates();
        }
      }
    }
  }
  else
  {  
    const RouteList* rl = outRoutes();
    for(ciRoute ir = rl->begin(); ir != rl->end(); ++ir)
    {
      if(ir->type == Route::TRACK_ROUTE)
        ir->track->updateInternalSoloStates();
    }
  }  
  
  _nodeTraversed = false; // Reset.
}


//---------------------------------------------------------
//   updateSoloStates
//---------------------------------------------------------

void MidiTrack::updateSoloStates(bool noDec)
{
  if(noDec && !_solo)
    return;
  
  _nodeTraversed = true;  // Anti circular mechanism.
  
  _tmpSoloChainTrack = this;
  _tmpSoloChainDoIns = false;
  _tmpSoloChainNoDec = noDec;
  updateSoloState();
  
  if(outPort() >= 0)
  {
    MidiPort* mp = &MusEGlobal::midiPorts[outPort()];
    MidiDevice *md = mp->device();
    if(md && md->isSynti())
      ((SynthI*)md)->updateInternalSoloStates();
      
    // Support Midi Port -> Audio Input solo chains. p4.0.14 Tim.
    const int chbits = 1 << outChannel();
    const RouteList* rl = mp->outRoutes();
    for(ciRoute ir = rl->begin(); ir != rl->end(); ++ir)
    {
      if(ir->type == Route::TRACK_ROUTE && ir->track && ir->track->type() == Track::AUDIO_INPUT && (ir->channel & chbits) )
      {
        ir->track->updateInternalSoloStates();
      }  
    }
  }
  
  _nodeTraversed = false; // Reset.
}


//---------------------------------------------------------
//   updateSoloStates
//---------------------------------------------------------

void AudioTrack::updateSoloStates(bool noDec)
{
  if(noDec && !_solo)
    return;
  
  _nodeTraversed = true;  // Anti circular mechanism.

  _tmpSoloChainTrack = this;
  _tmpSoloChainNoDec = noDec;
  updateSoloState();
  
  _tmpSoloChainDoIns = true;
  if(type() == AUDIO_SOFTSYNTH)
  {
    const MidiTrackList* ml = MusEGlobal::song->midis();
    for(ciMidiTrack im = ml->begin(); im != ml->end(); ++im)
    {
      MidiTrack* mt = *im;
      if(mt->outPort() >= 0 && mt->outPort() == ((SynthI*)this)->midiPort())
        mt->updateInternalSoloStates();
    }
  }
  
  {
    const RouteList* rl = inRoutes();
    for(ciRoute ir = rl->begin(); ir != rl->end(); ++ir)
    {
      if(ir->type == Route::TRACK_ROUTE)
        ir->track->updateInternalSoloStates();
      else  
      // Support Midi Port -> Audio Input solo chains. p4.0.14 Tim.
      if(ir->type == Route::MIDI_PORT_ROUTE)    
      {
        const MidiTrackList* ml = MusEGlobal::song->midis();
        for(ciMidiTrack im = ml->begin(); im != ml->end(); ++im)
        {
          MidiTrack* mt = *im;
          if(mt->outPort() == ir->midiPort && ((1 << mt->outChannel()) & ir->channel) )
            mt->updateInternalSoloStates();
        }
      }
    }
  }  
  _tmpSoloChainDoIns = false;
  {
    const RouteList* rl = outRoutes();
    for(ciRoute ir = rl->begin(); ir != rl->end(); ++ir)
    {
      if(ir->type == Route::TRACK_ROUTE)
        ir->track->updateInternalSoloStates();
    }
  }  
  
  _nodeTraversed = false; // Reset.
}


//---------------------------------------------------------
//   setMute
//---------------------------------------------------------

void Track::setMute(bool val)
      {
      _mute = val;
      }

//---------------------------------------------------------
//   setOff
//---------------------------------------------------------

void Track::setOff(bool val)
      {
      _off = val;
      }

//---------------------------------------------------------
//   applyTrackCtrls
//   If trackChans is 0, just process controllers only, not audio (do not 'run').
//---------------------------------------------------------

void AudioTrack::processTrackCtrls(unsigned pos, int trackChans, unsigned nframes, float** buffer)
{
  const unsigned long syncFrame = MusEGlobal::audio->curSyncFrame();
  const unsigned long min_per = (MusEGlobal::config.minControlProcessPeriod > nframes) ? nframes : MusEGlobal::config.minControlProcessPeriod;
  unsigned long sample = 0;

  const AutomationType at = automationType();
  const bool no_auto = !MusEGlobal::automation || at == AUTO_OFF;
  CtrlListList* cll = controller();
  CtrlList* vol_ctrl = 0;
  CtrlList* pan_ctrl = 0;
  {
    ciCtrlList icl = cll->find(AC_VOLUME);
    if(icl == cll->end())
      return;
    vol_ctrl = icl->second;
    icl = cll->find(AC_PAN);
    if(icl == cll->end())
      return;
    pan_ctrl = icl->second;
  }
  
  int cur_slice = 0;
  while(sample < nframes)
  {
    unsigned long nsamp = nframes - sample;
    const unsigned long slice_frame = pos + sample;

    // Process automation control values, while also determining the maximum acceptable
    //  size of this run. Further processing, from FIFOs for example, can lower the size
    //  from there, but this section determines where the next highest maximum frame
    //  absolutely needs to be for smooth playback of the controller value stream...
    //
    if(trackChans != 0 && !_prefader)  // Don't bother if we don't want to run, or prefader is on.
    {
      ciCtrlList icl = cll->begin();
      for(unsigned long k = 0; k < _controlPorts; ++k)
      {
        CtrlList* cl = (icl != cll->end() ? icl->second : NULL);
        CtrlInterpolate& ci = _controls[k].interp;
        // Always refresh the interpolate struct at first, since things may have changed.
        // Or if the frame is outside of the interpolate range - and eStop is not true.  // FIXME TODO: Be sure these comparisons are correct.
        if(cur_slice == 0 || (!ci.eStop && MusEGlobal::audio->isPlaying() &&
            (slice_frame < (unsigned long)ci.sFrame || (ci.eFrame != -1 && slice_frame >= (unsigned long)ci.eFrame)) ) )
        {
          if(cl && (unsigned long)cl->id() == k)
          {
            cl->getInterpolation(slice_frame, no_auto || !_controls[k].enCtrl, &ci);
            if(icl != cll->end())
              ++icl;
          }
          else
          {
            // No matching controller, or end. Just copy the current value into the interpolator.
            // Keep the current icl iterator, because since they are sorted by frames,
            //  if the IDs didn't match it means we can just let k catch up with icl.
            ci.sFrame   = 0;
            ci.eFrame   = -1;
            ci.sVal     = _controls[k].val;
            ci.eVal     = ci.sVal;
            ci.doInterp = false;
            ci.eStop    = false;
          }
        }
        else
        {
          if(ci.eStop && ci.eFrame != -1 && slice_frame >= (unsigned long)ci.eFrame)  // FIXME TODO: Get that comparison right.
          {
            // Clear the stop condition and set up the interp struct appropriately as an endless value.
            ci.sFrame   = 0; //ci->eFrame;
            ci.eFrame   = -1;
            ci.sVal     = ci.eVal;
            ci.doInterp = false;
            ci.eStop    = false;
          }
          if(icl != cll->end())
            ++icl;
        }

        if(MusEGlobal::audio->isPlaying())
        {
          unsigned long samps = nsamp;
          if(ci.eFrame != -1)
            samps = (unsigned long)ci.eFrame - slice_frame;
          if(samps < nsamp)
            nsamp = samps;
        }

#ifdef NODE_DEBUG_PROCESS
        fprintf(stderr, "AudioTrack::processTrackCtrls k:%lu sample:%lu frame:%lu nextFrame:%d nsamp:%lu \n", k, sample, frame, ci.eFrame, nsamp);
#endif
      }
    }

#ifdef NODE_DEBUG_PROCESS
    fprintf(stderr, "AudioTrack::processTrackCtrls sample:%lu nsamp:%lu\n", sample, nsamp);
#endif

    //
    // Process all control ring buffer items valid for this time period...
    //
    bool found = false;
    unsigned long frame = 0;
    unsigned long evframe;
    while(!_controlFifo.isEmpty())
    {
      ControlEvent v = _controlFifo.peek();
      // The events happened in the last period or even before that. Shift into this period with + n. This will sync with audio.
      // If the events happened even before current frame - n, make sure they are counted immediately as zero-frame.
      evframe = (syncFrame > v.frame + nframes) ? 0 : v.frame - syncFrame + nframes;

      // Protection. Observed this condition. Why? Supposed to be linear timestamps.
      if(found && evframe < frame)
      {
        fprintf(stderr, "AudioTrack::processTrackCtrls *** Error: evframe:%lu < frame:%lu idx:%lu val:%f unique:%d\n",
          evframe, v.frame, v.idx, v.value, v.unique);

        // No choice but to ignore it.
        _controlFifo.remove();               // Done with the ring buffer's item. Remove it.
        continue;
      }

      if(evframe >= nframes                                            // Next events are for a later period.
          || (!found && !v.unique && (evframe - sample >= nsamp))      // Next events are for a later run in this period. (Autom took prio.)
          || (found && !v.unique && (evframe - sample >= min_per)))    // Eat up events within minimum slice - they're too close.
        break;
      _controlFifo.remove();               // Done with the ring buffer's item. Remove it.

      if(v.idx >= _controlPorts) // Sanity check
        break;

      found = true;
      frame = evframe;

      if(trackChans != 0 && !_prefader)    // Only if we want to run, and prefader is off.
      {
        CtrlInterpolate* ci = &_controls[v.idx].interp;
        ci->eFrame = frame;
        ci->eVal   = v.value;
        ci->eStop  = true;
      }
      
      // Need to update the automation value, otherwise it overwrites later with the last automation value.
      setPluginCtrlVal(v.idx, v.value);
    }

    if(found && trackChans != 0 && !_prefader)    // If a control FIFO item was found, takes priority over automation controller stream.
      nsamp = frame - sample;

    if(sample + nsamp > nframes)    // Safety check.
      nsamp = nframes - sample;

    // TODO: Don't allow zero-length runs. This could/should be checked in the control loop instead.
    // Note this means it is still possible to get stuck in the top loop (at least for a while).
    if(nsamp != 0)
    {
      if(trackChans != 0 && !_prefader)
      {
        const CtrlInterpolate& vol_interp = _controls[AC_VOLUME].interp;
        const CtrlInterpolate& pan_interp = _controls[AC_PAN].interp;
        unsigned k;
        //const float up_fact = 1.002711275;      // 3.01.. dB / 256
        //const float down_fact = 0.997296056;
        const float up_fact = 1.003471749;      // 3.01.. dB / 200
        const float down_fact = 0.996540262;
        float *sp1, *sp2, *dp1, *dp2;
        sp1 = sp2 = dp1 = dp2 = NULL;
        float _volume, v, _pan, v1, v2;

        if(trackChans == 1)
        {
          float* sp = buffer[0] + sample;
          float* dp = outBuffers[0] + sample;
          sp1 = sp2 = sp;
          dp1 = outBuffersExtraMix[0] + sample;
          dp2 = outBuffersExtraMix[1] + sample;
          k = 0;
          if(vol_interp.doInterp && MusEGlobal::audio->isPlaying())
          {
            for( ; k < nsamp; ++k)
            {
              _volume = vol_ctrl->interpolate(slice_frame + k, vol_interp);  
              v = _volume * _gain;
              if(v > _curVolume)
              {
                if(_curVolume == 0.0)
                  _curVolume = 0.001;  // Kick-start it from zero at -30dB.
                _curVolume *= up_fact;
                if(_curVolume >= v)
                  _curVolume = v;
              }
              else
              if(v < _curVolume)
              {
                _curVolume *= down_fact;
                if(_curVolume <= v || _curVolume <= 0.001)  // Or if less than -30dB.
                  _curVolume = v;
              }
              *dp++ = *sp++ * _curVolume;
            }
            _controls[AC_VOLUME].val = _volume;    // Update the port.
          }
          else
          {
            if(vol_interp.doInterp) // And not playing...
              _volume = vol_ctrl->interpolate(pos, vol_interp);
            else
              _volume = vol_interp.sVal;
            _controls[AC_VOLUME].val = _volume;    // Update the port.
            v = _volume * _gain;
            if(v > _curVolume)
            {
              //fprintf(stderr, "A %f %f\n", v, _curVolume);  
              if(_curVolume == 0.0)
                _curVolume = 0.001;  // Kick-start it from zero at -30dB.
              for( ; k < nsamp; ++k)
              {
                _curVolume *= up_fact;
                if(_curVolume >= v)
                {
                  _curVolume = v;
                  break;
                }
                *dp++ = *sp++ * _curVolume;
              }
            }
            else
            if(v < _curVolume)
            {
              //fprintf(stderr, "B %f %f\n", v, _curVolume);  
              for( ; k < nsamp; ++k)
              {
                _curVolume *= down_fact;
                if(_curVolume <= v || _curVolume <= 0.001)  // Or if less than -30dB.
                {
                  _curVolume = v;
                  break;
                }
                *dp++ = *sp++ * _curVolume;
              }
            }
            for( ; k < nsamp; ++k)
              *dp++ = *sp++ * _curVolume;
          }
        }
        else
        if(trackChans >= 2)
        {
          sp1  = buffer[0] + sample;
          sp2  = buffer[1] + sample;
          dp1  = outBuffers[0] + sample;
          dp2  = outBuffers[1] + sample;
        }

        k = 0;
        if((vol_interp.doInterp || pan_interp.doInterp) && MusEGlobal::audio->isPlaying())
        {
          for( ; k < nsamp; ++k)
          {
            _volume = vol_ctrl->interpolate(slice_frame + k, vol_interp);
            v = _volume * _gain;
            _pan = pan_ctrl->interpolate(slice_frame + k, pan_interp);
            v1 = v * (1.0 - _pan);
            v2 = v * (1.0 + _pan);
            if(v1 > _curVol1)
            {
              //fprintf(stderr, "C %f %f \n", v1, _curVol1);  
              if(_curVol1 == 0.0)
                _curVol1 = 0.001;  // Kick-start it from zero at -30dB.
              _curVol1 *= up_fact;
              if(_curVol1 >= v1)
                _curVol1 = v1;
            }
            else
            if(v1 < _curVol1)
            {
              //fprintf(stderr, "D %f %f \n", v1, _curVol1);  
              _curVol1 *= down_fact;
              if(_curVol1 <= v1 || _curVol1 <= 0.001)  // Or if less than -30dB.
                _curVol1 = v1;
            }
            *dp1++ = *sp1++ * _curVol1;

            if(v2 > _curVol2)
            {
              //fprintf(stderr, "E %f %f \n", v2, _curVol2);  
              if(_curVol2 == 0.0)
                _curVol2 = 0.001;  // Kick-start it from zero at -30dB.
              _curVol2 *= up_fact;
              if(_curVol2 >= v2)
                _curVol2 = v2;
            }
            else
            if(v2 < _curVol2)
            {
              //fprintf(stderr, "F %f %f \n", v2, _curVol2);  
              _curVol2 *= down_fact;
              if(_curVol2 <= v2 || _curVol2 <= 0.001)   // Or if less than -30dB.
                _curVol2 = v2;
            }
            *dp2++ = *sp2++ * _curVol2;
          }
          _controls[AC_VOLUME].val = _volume;    // Update the ports.
          _controls[AC_PAN].val = _pan;          
        }
        else
        {
          if(vol_interp.doInterp)  // And not playing...
            _volume = vol_ctrl->interpolate(pos, vol_interp);
          else
            _volume = vol_interp.sVal;
          if(pan_interp.doInterp)  // And not playing...
            _pan = pan_ctrl->interpolate(pos, pan_interp);
          else
            _pan = pan_interp.sVal;
          _controls[AC_VOLUME].val = _volume;    // Update the ports.
          _controls[AC_PAN].val = _pan;
          v = _volume * _gain;
          v1  = v * (1.0 - _pan);
          v2  = v * (1.0 + _pan);
          if(v1 > _curVol1)
          {
            //fprintf(stderr, "C %f %f \n", v1, _curVol1);  
            if(_curVol1 == 0.0)
              _curVol1 = 0.001;  // Kick-start it from zero at -30dB.
            for( ; k < nsamp; ++k)
            {
              _curVol1 *= up_fact;
              if(_curVol1 >= v1)
              {
                _curVol1 = v1;
                break;
              }
              *dp1++ = *sp1++ * _curVol1;
            }
          }
          else
          if(v1 < _curVol1)
          {
            //fprintf(stderr, "D %f %f \n", v1, _curVol1);  
            for( ; k < nsamp; ++k)
            {
              _curVol1 *= down_fact;
              if(_curVol1 <= v1 || _curVol1 <= 0.001)  // Or if less than -30dB.
              {
                _curVol1 = v1;
                break;
              }
              *dp1++ = *sp1++ * _curVol1;
            }
          }
          for( ; k < nsamp; ++k)
            *dp1++ = *sp1++ * _curVol1;

          k = 0;
          if(v2 > _curVol2)
          {
            //fprintf(stderr, "E %f %f \n", v2, _curVol2);  
            if(_curVol2 == 0.0)
              _curVol2 = 0.001;  // Kick-start it from zero at -30dB.
            for( ; k < nsamp; ++k)
            {
              _curVol2 *= up_fact;
              if(_curVol2 >= v2)
              {
                _curVol2 = v2;
                break;
              }
              *dp2++ = *sp2++ * _curVol2;
            }
          }
          else
          if(v2 < _curVol2)
          {
            //fprintf(stderr, "F %f %f \n", v2, _curVol2);  
            for( ; k < nsamp; ++k)
            {
              _curVol2 *= down_fact;
              if(_curVol2 <= v2 || _curVol2 <= 0.001)   // Or if less than -30dB.
              {
                _curVol2 = v2;
                break;
              }
              *dp2++ = *sp2++ * _curVol2;
            }
          }
          for( ; k < nsamp; ++k)
            *dp2++ = *sp2++ * _curVol2;
        }
      }

      sample += nsamp;
    }  

    ++cur_slice; // Slice is done. Moving on to any next slice now...
  }
}

//---------------------------------------------------------
//   copyData
//---------------------------------------------------------

// this is also addData(). addData() just calls copyData(..., true);
void AudioTrack::copyData(unsigned pos, int dstChannels, int srcStartChan, int srcChannels, unsigned nframes, float** dstBuffer, bool add)
{
  //Changed by T356. 12/12/09. 
  // Overhaul and streamline to eliminate multiple processing during one process loop. 
  // Was causing ticking sound with synths + multiple out routes because synths were being processed multiple times.
  // Make better use of AudioTrack::outBuffers as a post-effect pre-volume cache system for multiple calls here during processing.
  // Previously only WaveTrack used them. (Changed WaveTrack as well).
  
  #ifdef NODE_DEBUG_PROCESS
  printf("MusE: AudioTrack::copyData name:%s processed:%d\n", name().toLatin1().constData(), processed());
  #endif
  
  if(srcStartChan == -1)
    srcStartChan = 0;
    
  const int trackChans = channels();
  int srcChans = (srcChannels == -1) ? trackChans : srcChannels;
  const int srcTotalOutChans = (channels() == 1) ? 1 : totalOutChannels();
  
  // Special consideration for metronome: It is not part of the track list,
  //  and it has no in or out routes, yet multiple output tracks may call addData on it!
  // We can't tell how many output tracks call it, so we can only assume there might be more than one.
  // Not strictly necessary here because only addData is ever called, but just to be consistent...
  
  int i;

  float* buffer[srcTotalOutChans];  
  float data[nframes * srcTotalOutChans];
  float meter[trackChans];

  // Have we been here already during this process cycle?
  if(processed())
  {
    #ifdef NODE_DEBUG_PROCESS
    printf("MusE: AudioTrack::copyData name:%s already processed _haveData:%d\n", name().toLatin1().constData(), _haveData);
    #endif
    
    // Is there already some data gathered from a previous call during this process cycle?
    if(_haveData)
    {
      if(srcChans == dstChannels)
      {
        for(int c = 0; c < dstChannels; ++c)
        {
          float* sp = outBuffers[c + srcStartChan];
          float* dp = dstBuffer[c];
          if (!add)
            AL::dsp->cpy(dp, sp, nframes);
          else 
            for(unsigned k = 0; k < nframes; ++k)
              *dp++ += *sp++;
        }
      }
      else if(srcChans == 1 && dstChannels == 2)
      {
        for(int c = 0; c < dstChannels; ++c)
        {
          float* sp;
          if(!_prefader && srcStartChan == 0 && trackChans == 1)
            sp = outBuffersExtraMix[c];  // Use the pre-panned mono-to-stereo extra buffers.
          else
            sp = outBuffers[srcStartChan]; // In all other cases use the main buffers.
          float* dp = dstBuffer[c];
          if (!add)
            AL::dsp->cpy(dp, sp, nframes);
          else 
            for(unsigned k = 0; k < nframes; ++k)
              *dp++ += *sp++;
        }
      }
      else if(srcChans == 2 && dstChannels == 1)
      {
        float* dp = dstBuffer[0];
        float* sp1 = outBuffers[srcStartChan];
        float* sp2 = outBuffers[srcStartChan + 1];
        if (!add)
          for(unsigned k = 0; k < nframes; ++k)
            *dp++ = (*sp1++ + *sp2++);
        else 
          for(unsigned k = 0; k < nframes; ++k)
            *dp++ += (*sp1++ + *sp2++);
      }
    }
    else
    {
      // No data was available from a previous call during this process cycle. 
      
      if (!add)
      {
          //Zero the supplied buffers and just return.
          for(i = 0; i < dstChannels; ++i) 
          {
            if(MusEGlobal::config.useDenormalBias) 
            {
              for(unsigned int q = 0; q < nframes; ++q)
                dstBuffer[i][q] = MusEGlobal::denormalBias;
            } 
            else
              memset(dstBuffer[i], 0, sizeof(float) * nframes);
          }
      }
    }
    return;
  }
  else 
  {
    // First time here during this process cycle. 
    
    _haveData = false;  // Reset.
    _processed = true;  // Set this now.

    if(off())  
    {  
      #ifdef NODE_DEBUG_PROCESS
      printf("MusE: AudioTrack::copyData name:%s dstChannels:%d Off, zeroing buffers\n", name().toLatin1().constData(), dstChannels);
      #endif
      
      if(!add)
      {
          // Track is off. Zero the supplied buffers.
          unsigned int q;
          for(i = 0; i < dstChannels; ++i) 
          {
            if(MusEGlobal::config.useDenormalBias) 
            {
              for(q = 0; q < nframes; ++q)
                dstBuffer[i][q] = MusEGlobal::denormalBias;
            } 
            else
              memset(dstBuffer[i], 0, sizeof(float) * nframes);
          }  
      }
     
      _efxPipe->apply(pos, 0, nframes, 0);  // Just process controls only, not audio (do not 'run').    
      processTrackCtrls(pos, 0, nframes, 0);

      for(i = 0; i < trackChans; ++i) 
        _meter[i] = 0.0;
      
      return;
    }
    
    // Point the input buffers at a temporary stack buffer.
    for(i = 0; i < srcTotalOutChans; ++i)  
        buffer[i] = data + i * nframes;
  
    // getData can use the supplied buffers, or change buffer to point to its own local buffers or Jack buffers etc. 
    // For ex. if this is an audio input, Jack will set the pointers for us in AudioInput::getData!
    // Don't do any processing at all if off. Whereas, mute needs to be ready for action at all times,
    //  so still call getData before it. Off is NOT meant to be toggled rapidly, but mute is !
    if(!getData(pos, srcTotalOutChans, nframes, buffer))
    {
      #ifdef NODE_DEBUG_PROCESS
      printf("MusE: AudioTrack::copyData name:%s srcTotalOutChans:%d zeroing buffers\n", name().toLatin1().constData(), srcTotalOutChans);
      #endif
      
      // No data was available. Track is not off. Zero the working buffers and continue on.
      unsigned int q;
      for(i = 0; i < srcTotalOutChans; ++i)
      {  
        if(MusEGlobal::config.useDenormalBias) 
        {
          for(q = 0; q < nframes; ++q)
            buffer[i][q] = MusEGlobal::denormalBias;
        } 
        else
          memset(buffer[i], 0, sizeof(float) * nframes);
      }  
    }

    //---------------------------------------------------
    // apply plugin chain
    //---------------------------------------------------

    // Allow it to process even if muted so that when mute is turned off, left-over buffers (reverb tails etc) can die away.
    _efxPipe->apply(pos, trackChans, nframes, buffer);

    //---------------------------------------------------
    // apply volume, pan
    //---------------------------------------------------

    #ifdef NODE_DEBUG_PROCESS
    printf("MusE: AudioTrack::copyData trackChans:%d srcTotalOutChans:%d srcStartChan:%d srcChans:%d dstChannels:%d\n", trackChans, srcTotalOutChans, srcStartChan, srcChans, dstChannels);
    #endif

    processTrackCtrls(pos, trackChans, nframes, buffer);

    const int valid_out_bufs = _prefader ? 0 : (trackChans >= 2 ? 2 : trackChans);

    //---------------------------------------------------
    //    metering
    //---------------------------------------------------

    for(int c = 0; c < trackChans; ++c)
    {
      meter[c] = 0.0;
      float* sp = (c >= valid_out_bufs) ? buffer[c] : outBuffers[c]; // Optimize: Don't all valid outBuffers just for meters
      for(unsigned k = 0; k < nframes; ++k)
      {
        const double f = fabs(*sp++); // If the track is mono pan has no effect on meters.
        if(f > meter[c])
          meter[c] = f;
      }
      _meter[c] = meter[c];
      if(_meter[c] > _peak[c])
        _peak[c] = _meter[c];
    }
    
    if(isMute())
    {
      if (!add)
      {
          unsigned int q;
          for(i = 0; i < dstChannels; ++i)
          {
            if(MusEGlobal::config.useDenormalBias)
            {
              for(q = 0; q < nframes; q++)
                dstBuffer[i][q] = MusEGlobal::denormalBias;
            }
            else
              memset(dstBuffer[i], 0, sizeof(float) * nframes);
          }
      }
      return; // We're outta here.
    }

    // Copy whole blocks that we can get away with here outside of the track control processing loop. 
    for(i = valid_out_bufs; i < srcTotalOutChans; ++i)
      AL::dsp->cpy(outBuffers[i], buffer[i], nframes);

    // We now have some data! Set to true.
    _haveData = true;
      
    //---------------------------------------------------
    // aux sends
    //---------------------------------------------------

    if(hasAuxSend())
    {
      AuxList* al = MusEGlobal::song->auxs();
      unsigned naux = al->size();
      for(unsigned k = 0; k < naux; ++k)
      {
        float m = _auxSend[k];
        if(m <= 0.0001)           // optimize
          continue;
        AudioAux* a = (AudioAux*)((*al)[k]);
        float** dst = a->sendBuffer();
        int auxChannels = a->channels();
        if((srcChans ==1 && auxChannels==1) || srcChans == 2)
        {
          for(int ch = 0; ch < srcChans; ++ch)
          {
            float* db = dst[ch % a->channels()]; // no matter whether there's one or two dst buffers
            float* sb = outBuffers[ch];
            for(unsigned f = 0; f < nframes; ++f)
              *db++ += (*sb++ * m);   // add to mix
          }
        }
        else if(srcChans==1 && auxChannels==2)  // copy mono to both channels
        {
          for(int ch = 0; ch < auxChannels; ++ch)
          {
            float* db = dst[ch % a->channels()];
            float* sb = outBuffers[0];
            for(unsigned f = 0; f < nframes; ++f)
              *db++ += (*sb++ * m);   // add to mix
          }
        }
      }
    }

    //---------------------------------------------------
    //    copy to destination buffers
    //---------------------------------------------------

    // Sanity check. Is source starting channel out of range? Just zero and return.
    if(srcStartChan >= srcTotalOutChans)
    {
      if(!add)
      {
        unsigned int q;
        for(i = 0; i < dstChannels; ++i)
        {
          if(MusEGlobal::config.useDenormalBias)
          {
            for(q = 0; q < nframes; q++)
              dstBuffer[i][q] = MusEGlobal::denormalBias;
          }
          else
            memset(dstBuffer[i], 0, sizeof(float) * nframes);
        }
      }
      return;
    }

    // Force a source range to fit actual available total out channels.
    if((srcStartChan + srcChans) > srcTotalOutChans)
      srcChans = srcTotalOutChans - srcStartChan;

    if(srcChans == dstChannels)
    {
      for(int c = 0; c < dstChannels; ++c)
      {
        float* sp = outBuffers[c + srcStartChan];
        float* dp = dstBuffer[c];
        if (!add)
          AL::dsp->cpy(dp, sp, nframes);
        else
          for(unsigned k = 0; k < nframes; ++k)
            *dp++ += *sp++;
      }
    }
    else if(srcChans == 1 && dstChannels == 2)
    {
      for(int c = 0; c < dstChannels; ++c)
      {
        float* sp;
        if(!_prefader && srcStartChan == 0 && trackChans == 1)
          sp = outBuffersExtraMix[c];  // Use the pre-panned mono-to-stereo extra buffers.
        else
          sp = outBuffers[srcStartChan]; // In all other cases use the main buffers.
        float* dp = dstBuffer[c];
        if (!add)
          AL::dsp->cpy(dp, sp, nframes);
        else
          for(unsigned k = 0; k < nframes; ++k)
            *dp++ += *sp++;
      }
    }
    else if(srcChans == 2 && dstChannels == 1)
    {
      float* dp = dstBuffer[0];
      float* sp1 = outBuffers[srcStartChan];
      float* sp2 = outBuffers[srcStartChan + 1];
      if (!add)
        for(unsigned k = 0; k < nframes; ++k)
          *dp++ = (*sp1++ + *sp2++);
      else
        for(unsigned k = 0; k < nframes; ++k)
          *dp++ += (*sp1++ + *sp2++);
    }
  }
}

//---------------------------------------------------------
//   addData
//---------------------------------------------------------

void AudioTrack::addData(unsigned pos, int dstChannels, int srcStartChan, int srcChannels, unsigned nframes, float** dstBuffer)
{
    copyData(pos,dstChannels,srcStartChan,srcChannels,nframes,dstBuffer, true);
}

//---------------------------------------------------------
//   readVolume
//---------------------------------------------------------

void AudioTrack::readVolume(Xml& xml)
      {
      for (;;) {
            Xml::Token token = xml.parse();
            switch (token) {
                  case Xml::Error:
                  case Xml::End:
                        return;
                  case Xml::TagStart:
                        xml.unknown("readVolume");
                        break;
                  case Xml::Text:
                        setVolume(xml.s1().toDouble());
                        break;
                  case Xml::Attribut:
                        if (xml.s1() == "ch")
                              //ch = xml.s2().toInt();
                              xml.s2();
                        break;
                  case Xml::TagEnd:
                        if (xml.s1() == "volume")
                              return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   setChannels
//---------------------------------------------------------

void Track::setChannels(int n)
      {
      if(n > MAX_CHANNELS)
        _channels = MAX_CHANNELS;
      else  
        _channels = n;
      for (int i = 0; i < _channels; ++i) {
            _meter[i] = 0.0;
            _peak[i]  = 0.0;
            }
      }


void AudioInput::setChannels(int n)
      {
      if (n == _channels)
            return;
      AudioTrack::setChannels(n);
      }

void AudioOutput::setChannels(int n)
      {
      if (n == _channels)
            return;
      AudioTrack::setChannels(n);
      }

//---------------------------------------------------------
//   putFifo
//---------------------------------------------------------

void AudioTrack::putFifo(int channels, unsigned long n, float** bp)
      {
      if (fifo.put(channels, n, bp, MusEGlobal::audio->pos().frame())) {
            printf("   overrun ???\n");
            }
      }

//---------------------------------------------------------
//   getData
//    return false if no data available
//---------------------------------------------------------

bool AudioTrack::getData(unsigned pos, int channels, unsigned nframes, float** buffer)
      {
      // use supplied buffers

      RouteList* rl = inRoutes();
      
      #ifdef NODE_DEBUG_PROCESS
      printf("AudioTrack::getData name:%s inRoutes:%lu\n", name().toLatin1().constData(), rl->size());
      #endif
      
      ciRoute ir = rl->begin();
      if (ir == rl->end())
            return false;
      
      if(ir->track->isMidiTrack())
        return false;
        
      #ifdef NODE_DEBUG_PROCESS
      printf("    calling copyData on %s...\n", ir->track->name().toLatin1().constData());
      #endif
      
      ((AudioTrack*)ir->track)->copyData(pos, channels, 
                                         ir->channel,
                                         ir->channels,
                                         nframes, buffer);
      
      
      ++ir;
      for (; ir != rl->end(); ++ir) {
            #ifdef NODE_DEBUG_PROCESS
            printf("    calling addData on %s...\n", ir->track->name().toLatin1().constData());
            #endif
              
            if(ir->track->isMidiTrack())
              continue;
              
            ((AudioTrack*)ir->track)->addData(pos, channels, 
                                              //(ir->track->type() == Track::AUDIO_SOFTSYNTH && ir->channel != -1) ? ir->channel : 0,
                                              ir->channel,
                                              ir->channels,
                                              nframes, buffer);
            }
      return true;
      }

//---------------------------------------------------------
//   getData
//    return true if data
//---------------------------------------------------------

bool AudioInput::getData(unsigned, int channels, unsigned nframes, float** buffer)
      {
      if (!MusEGlobal::checkAudioDevice()) return false;
      for (int ch = 0; ch < channels; ++ch) 
      {
            void* jackPort = jackPorts[ch];

            // REMOVE Tim. Just a test.
            //if(jackPort)
            //  MusEGlobal::audioDevice->portLatency(jackPort, true);
            
            // Do not get buffers of unconnected client ports. Causes repeating leftover data, can be loud, or DC !
            if (jackPort && MusEGlobal::audioDevice->connections(jackPort)) 
            {
                  //buffer[ch] = MusEGlobal::audioDevice->getBuffer(jackPort, nframes);
                  // If the client port buffer is also used by another channel (connected to the same jack port), 
                  //  don't directly set pointer, copy the data instead. 
                  // Otherwise the next channel will interfere - it will overwrite the buffer !
                  // Verified symptoms: Can't use a splitter. Mono noise source on a stereo track sounds in mono. Etc...
                  // TODO: Problem: What if other Audio Input tracks share the same jack ports as this Audio Input track?
                  // Users will expect that Audio Inputs just work even if the input routes originate from the same jack port.
                  // Solution: Rather than having to iterate all other channels, and all other Audio Input tracks and check 
                  //  their channel port buffers (if that's even possible) in order to determine if the buffer is shared, 
                  //  let's just copy always, for now shall we ?
                  float* jackbuf = MusEGlobal::audioDevice->getBuffer(jackPort, nframes);
                  AL::dsp->cpy(buffer[ch], jackbuf, nframes);
                  
                  if (MusEGlobal::config.useDenormalBias) 
                  {
                      for (unsigned int i=0; i < nframes; i++)
                              buffer[ch][i] += MusEGlobal::denormalBias;
                  }
            } 
            else 
            {
                  if (MusEGlobal::config.useDenormalBias) 
                  {
                      for (unsigned int i=0; i < nframes; i++)
                              buffer[ch][i] = MusEGlobal::denormalBias;
                  } 
                  else 
                  {
                              memset(buffer[ch], 0, nframes * sizeof(float));
                  }
            }
      }
      return true;
}

//---------------------------------------------------------
//   setName
//---------------------------------------------------------

void AudioInput::setName(const QString& s)
      {
      _name = s;
      if (!MusEGlobal::checkAudioDevice()) return;
      for (int i = 0; i < channels(); ++i) {
            char buffer[128];
            snprintf(buffer, 128, "%s-%d", _name.toLatin1().constData(), i);
            if (jackPorts[i])
                  MusEGlobal::audioDevice->setPortName(jackPorts[i], buffer);
            else {
                  jackPorts[i] = MusEGlobal::audioDevice->registerInPort(buffer, false);
                  }
            }
      }


//---------------------------------------------------------
//   resetMeter
//---------------------------------------------------------

void Track::resetMeter()
      {
      for (int i = 0; i < _channels; ++i)
            _meter[i] = 0.0;
      }

//---------------------------------------------------------
//   resetPeaks
//---------------------------------------------------------

void Track::resetPeaks()
      {
      for (int i = 0; i < _channels; ++i)
            _peak[i] = 0.0;
            _lastActivity = 0;
      }

//---------------------------------------------------------
//   resetAllMeter
//---------------------------------------------------------

void Track::resetAllMeter()
      {
      TrackList* tl = MusEGlobal::song->tracks();
      for (iTrack i = tl->begin(); i != tl->end(); ++i)
            (*i)->resetMeter();
      }


//---------------------------------------------------------
//   setRecordFlag2
//    real time part (executed in audio thread)
//---------------------------------------------------------

void AudioTrack::setRecordFlag2(bool f)
      {
      if (f == _recordFlag)
            return;
      _recordFlag = f;
      if (!_recordFlag)
            resetMeter();
      }

//---------------------------------------------------------
//   setMute
//---------------------------------------------------------

void AudioTrack::setMute(bool f)
      {
      _mute = f;
      if (_mute)
            resetAllMeter();
      }

//---------------------------------------------------------
//   setOff
//---------------------------------------------------------

void AudioTrack::setOff(bool val)
      {
      _off = val;
      if (val)
            resetAllMeter();
      }

//---------------------------------------------------------
//   setPrefader
//---------------------------------------------------------

void AudioTrack::setPrefader(bool val)
      {
      _prefader = val;
      if (!_prefader && isMute())
            resetAllMeter();
      }


//---------------------------------------------------------
//   canEnableRecord
//---------------------------------------------------------

bool WaveTrack::canEnableRecord() const
      {
      return  (!noInRoute() || (this == MusEGlobal::song->bounceTrack));
      }


//---------------------------------------------------------
//   record
//---------------------------------------------------------

void AudioTrack::record()
      {
      unsigned pos = 0;
      float* buffer[_channels];

      while(fifo.getCount()) {

            if (fifo.get(_channels, MusEGlobal::segmentSize, buffer, &pos)) {
                  printf("AudioTrack::record(): empty fifo\n");
                  return;
                  }
            if (_recFile) {
                  // Line removed by Tim. Oct 28, 2009
                  //_recFile->seek(pos, 0);
                  //
                  // Fix for recorded waves being shifted ahead by an amount
                  //  equal to start record position.
                  //
                  // From libsndfile ChangeLog:
                  // 2008-05-11  Erik de Castro Lopo  <erikd AT mega-nerd DOT com>
                  //    * src/sndfile.c
                  //    Allow seeking past end of file during write.
                  //    
                  // I don't know why this line would even be called, because the FIFOs'
                  //  'pos' members operate in absolute frames, which at this point 
                  //  would be shifted ahead by the start of the wave part.
                  // So if you begin recording a new wave part at bar 4, for example, then
                  //  this line is seeking the record file to frame 288000 even before any audio is written!
                  // Therefore, just let the write do its thing and progress naturally,
                  //  it should work OK since everything was OK before the libsndfile change...
                  //
                  // Tested: With the line, audio record looping sort of works, albiet with the start offset added to
                  //  the wave file. And it overwrites existing audio. (Check transport window 'overwrite' function. Tie in somehow...)
                  // With the line, looping does NOT work with libsndfile from around early 2007 (my distro's version until now).
                  // Therefore it seems sometime between libsndfile ~2007 and today, libsndfile must have allowed 
                  //  "seek (behind) on write", as well as the "seek past end" change of 2008...
                  //
                  // Ok, so removing that line breaks *possible* record audio 'looping' functionality, revealed with
                  //  later libsndfile. 
                  // Try this... And while we're at it, honour the punchin/punchout, and loop functions !
                  //
                  // If punchin is on, or we have looped at least once, use left marker as offset.
                  // Note that audio::startRecordPos is reset to (roughly) the left marker pos upon loop !
                  // (Not any more! I changed Audio::Process)
                  // Since it is possible to start loop recording before the left marker (with punchin off), we must 
                  //  use startRecordPos or loopFrame or left marker, depending on punchin and whether we have looped yet.
                  unsigned fr;
                  if(MusEGlobal::song->punchin() && (MusEGlobal::audio->loopCount() == 0))
                    fr = MusEGlobal::song->lPos().frame();
                  else  
                  if((MusEGlobal::audio->loopCount() > 0) && (MusEGlobal::audio->getStartRecordPos().frame() > MusEGlobal::audio->loopFrame()))
                    fr = MusEGlobal::audio->loopFrame();
                  else
                    fr = MusEGlobal::audio->getStartRecordPos().frame();
                  // Now seek and write. If we are looping and punchout is on, don't let punchout point interfere with looping point.
                  if( (pos >= fr) && (!MusEGlobal::song->punchout() || (!MusEGlobal::song->loop() && pos < MusEGlobal::song->rPos().frame())) )
                  {
                    pos -= fr;
                    
                    _recFile->seek(pos, 0);
                    _recFile->write(_channels, buffer, MusEGlobal::segmentSize);
                  }
                    
                  }
            else {
                  printf("AudioNode::record(): no recFile\n");
                  }
            }
      }

//---------------------------------------------------------
//   processInit
//---------------------------------------------------------

void AudioOutput::processInit(unsigned nframes)
      {
      _nframes = nframes;
      if (!MusEGlobal::checkAudioDevice()) return;
      for (int i = 0; i < channels(); ++i) {
            if (jackPorts[i]) {

                  //MusEGlobal::audioDevice->portLatency(jackPorts[i], true);  // REMOVE Tim. Just a test.
              
                  buffer[i] = MusEGlobal::audioDevice->getBuffer(jackPorts[i], nframes);
                  if (MusEGlobal::config.useDenormalBias) {
                      for (unsigned int j=0; j < nframes; j++)
                              buffer[i][j] += MusEGlobal::denormalBias;
                      }
                  }
            else
                  printf("PANIC: processInit: no buffer from audio driver\n");
            }
      }

//---------------------------------------------------------
//   process
//    synthesize "n" frames at buffer offset "offset"
//    current frame position is "pos"
//---------------------------------------------------------

void AudioOutput::process(unsigned pos, unsigned offset, unsigned n)
{
      #ifdef NODE_DEBUG_PROCESS
      printf("MusE: AudioOutput::process name:%s processed:%d\n", name().toLatin1().constData(), processed());
      #endif
      
      for (int i = 0; i < _channels; ++i) {
            buffer1[i] = buffer[i] + offset;
      }
      copyData(pos, _channels, -1, -1, n, buffer1);
}

//---------------------------------------------------------
//   silence
//---------------------------------------------------------

void AudioOutput::silence(unsigned n)
      {
      processInit(n);
      for (int i = 0; i < channels(); ++i)
          if (MusEGlobal::config.useDenormalBias) {
              for (unsigned int j=0; j < n; j++)
                  buffer[i][j] = MusEGlobal::denormalBias;
            } else {
                  memset(buffer[i], 0, n * sizeof(float));
                  }
      }

//---------------------------------------------------------
//   processWrite
//---------------------------------------------------------

void AudioOutput::processWrite()
      {
      if (MusEGlobal::audio->isRecording() && MusEGlobal::song->bounceOutput == this) {
            if (MusEGlobal::audio->freewheel()) {
                  MusECore::WaveTrack* track = MusEGlobal::song->bounceTrack;
                  if (track && track->recordFlag() && track->recFile())
                        track->recFile()->write(_channels, buffer, _nframes);
                  if (recordFlag() && recFile())
                        _recFile->write(_channels, buffer, _nframes);
                  }
            else {
                  MusECore::WaveTrack* track = MusEGlobal::song->bounceTrack;
                  if (track && track->recordFlag() && track->recFile())
                        track->putFifo(_channels, _nframes, buffer);
                  if (recordFlag() && recFile())
                        putFifo(_channels, _nframes, buffer);
                  }
            }
      if (sendMetronome() && MusEGlobal::audioClickFlag && MusEGlobal::song->click()) {
            
            #ifdef METRONOME_DEBUG
            printf("MusE: AudioOutput::processWrite Calling metronome->addData frame:%u channels:%d frames:%lu\n", MusEGlobal::audio->pos().frame(), _channels, _nframes);
            #endif
            metronome->addData(MusEGlobal::audio->pos().frame(), _channels, -1, -1, _nframes, buffer);
            }

            MusEGlobal::wavePreview->addData(_channels, _nframes, buffer);
      }
//---------------------------------------------------------
//   setName
//---------------------------------------------------------

void AudioOutput::setName(const QString& s)
      {
      _name = s;
      if (!MusEGlobal::checkAudioDevice()) return;
      for (int i = 0; i < channels(); ++i) {
            char buffer[128];
            snprintf(buffer, 128, "%s-%d", _name.toLatin1().constData(), i);
            if (jackPorts[i])
                  MusEGlobal::audioDevice->setPortName(jackPorts[i], buffer);
            else
                  jackPorts[i] = MusEGlobal::audioDevice->registerOutPort(buffer, false);
            }
      }


//---------------------------------------------------------
//   Fifo
//---------------------------------------------------------

Fifo::Fifo()
      {
      muse_atomic_init(&count);
      nbuffer = MusEGlobal::fifoLength;
      buffer  = new FifoBuffer*[nbuffer];
      for (int i = 0; i < nbuffer; ++i)
            buffer[i]  = new FifoBuffer;
      clear();
      }

Fifo::~Fifo()
      {
      for (int i = 0; i < nbuffer; ++i)
      {
        if(buffer[i]->buffer)
          free(buffer[i]->buffer);
          
        delete buffer[i];
      }
            
      delete[] buffer;
      muse_atomic_destroy(&count);
      }

//---------------------------------------------------------
//   put
//    return true if fifo full
//---------------------------------------------------------

bool Fifo::put(int segs, unsigned long samples, float** src, unsigned pos)
      {
      #ifdef FIFO_DEBUG
      printf("FIFO::put segs:%d samples:%lu pos:%u\n", segs, samples, pos);
      #endif
      
      if (muse_atomic_read(&count) == nbuffer) {
            printf("FIFO %p overrun... %d\n", this, muse_atomic_read(&count));
            return true;
            }
      FifoBuffer* b = buffer[widx];
      int n         = segs * samples;
      if (b->maxSize < n) {
            if (b->buffer)
            {
              free(b->buffer);
              b->buffer = 0;
            }     
            int rv = posix_memalign((void**)&(b->buffer), 16, sizeof(float) * n);
            if(rv != 0 || !b->buffer)
            {
              printf("Fifo::put could not allocate buffer segs:%d samples:%lu pos:%u\n", segs, samples, pos);
              return true;
            }
            
            b->maxSize = n;
            }
      if(!b->buffer)
      {
        printf("Fifo::put no buffer! segs:%d samples:%lu pos:%u\n", segs, samples, pos);
        return true;
      }
      
      b->size = samples;
      b->segs = segs;
      b->pos  = pos;
      for (int i = 0; i < segs; ++i)
            AL::dsp->cpy(b->buffer + i * samples, src[i], samples);
      add();
      return false;
      }

//---------------------------------------------------------
//   get
//    return true if fifo empty
//---------------------------------------------------------

bool Fifo::get(int segs, unsigned long samples, float** dst, unsigned* pos)
      {
      #ifdef FIFO_DEBUG
      printf("FIFO::get segs:%d samples:%lu\n", segs, samples);
      #endif
      
      if (muse_atomic_read(&count) == 0) {
            printf("FIFO %p underrun... %d\n", this, muse_atomic_read(&count)); //by willyfoobar: added count to output //see Fifo::put()
            return true;
            }
      FifoBuffer* b = buffer[ridx];
      if(!b->buffer)
      {
        printf("Fifo::get no buffer! segs:%d samples:%lu b->pos:%u\n", segs, samples, b->pos);
        return true;
      }
      
      if (pos)
            *pos = b->pos;
      
      for (int i = 0; i < segs; ++i)
            dst[i] = b->buffer + samples * (i % b->segs);
      remove();
      return false;
      }

int Fifo::getCount()
      {
      return muse_atomic_read(&count);
      }
//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Fifo::remove()
      {
      ridx = (ridx + 1) % nbuffer;
      muse_atomic_dec(&count);
      }

//---------------------------------------------------------
//   getWriteBuffer
//---------------------------------------------------------

bool Fifo::getWriteBuffer(int segs, unsigned long samples, float** buf, unsigned pos)
      {
      #ifdef FIFO_DEBUG
      printf("Fifo::getWriteBuffer segs:%d samples:%lu pos:%u\n", segs, samples, pos);
      #endif
      
      if (muse_atomic_read(&count) == nbuffer)
            return true;
      FifoBuffer* b = buffer[widx];
      int n = segs * samples;
      if (b->maxSize < n) {
            if (b->buffer)
            {
              free(b->buffer);
              b->buffer = 0;
            }
            
            int rv = posix_memalign((void**)&(b->buffer), 16, sizeof(float) * n);
            if(rv != 0 || !b->buffer)
            {
              printf("Fifo::getWriteBuffer could not allocate buffer segs:%d samples:%lu pos:%u\n", segs, samples, pos);
              return true;
            }
            
            b->maxSize = n;
            }
      if(!b->buffer)
      {
        printf("Fifo::getWriteBuffer no buffer! segs:%d samples:%lu pos:%u\n", segs, samples, pos);
        return true;
      }
      
      for (int i = 0; i < segs; ++i)
            buf[i] = b->buffer + i * samples;
            
      b->size = samples;
      b->segs = segs;
      b->pos  = pos;
      return false;
      }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Fifo::add()
      {
      widx = (widx + 1) % nbuffer;
      muse_atomic_inc(&count);
      }

//---------------------------------------------------------
//   setParam
//---------------------------------------------------------

void AudioTrack::setParam(unsigned long i, float val)
{
  addScheduledControlEvent(i, val, MusEGlobal::audio->curFrame());
}

//---------------------------------------------------------
//   param
//---------------------------------------------------------

float AudioTrack::param(unsigned long i) const
{
  return _controls[i].val;
}

//---------------------------------------------------------
//   setChannels
//---------------------------------------------------------

void AudioTrack::setChannels(int n)
      {
      Track::setChannels(n);
      if (_efxPipe)
            _efxPipe->setChannels(n);
      }

//---------------------------------------------------------
//   setTotalOutChannels
//---------------------------------------------------------

void AudioTrack::setTotalOutChannels(int num)
{
      int chans = _totalOutChannels;
      if(num != chans)  
      {
        _totalOutChannels = num;
        int new_chans = num;
        // Number of allocated buffers is always MAX_CHANNELS or more, even if _totalOutChannels is less.
        if(new_chans < MAX_CHANNELS)
          new_chans = MAX_CHANNELS;
        if(chans < MAX_CHANNELS)
          chans = MAX_CHANNELS;
        if(new_chans != chans)
        {
          if(outBuffers)
          {
            for(int i = 0; i < chans; ++i)
            {
              if(outBuffers[i])
              {
                free(outBuffers[i]);
                outBuffers[i] = NULL;
              }
            }
            delete[] outBuffers;
            outBuffers = NULL;
          }
        }
        initBuffers();
      }  
      chans = num;
      // Limit the actual track (meters, copying etc, all 'normal' operation) to two-channel stereo.
      if(chans > MAX_CHANNELS)
        chans = MAX_CHANNELS;
      setChannels(chans);
}

//---------------------------------------------------------
//   setTotalInChannels
//---------------------------------------------------------

void AudioTrack::setTotalInChannels(int num)
{
      if(num == _totalInChannels)
        return;
        
      _totalInChannels = num;
}

} // namespace MusECore
