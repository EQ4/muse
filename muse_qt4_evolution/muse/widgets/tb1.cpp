//=============================================================================
//  MusE
//  Linux Music Editor
//  $Id:$
//
//  Copyright (C) 2002-2006 by Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "tb1.h"
#include "globals.h"
#include "awl/poslabel.h"
#include "awl/pitchlabel.h"
#include "rastercombo.h"
#include "quantcombo.h"

//---------------------------------------------------------
//   genToolbar
//    solo time pitch raster quant
//---------------------------------------------------------

Toolbar1::Toolbar1(int r, int q, bool showPitch)
      {
      solo = new QToolButton;
      solo->setText(tr("Solo"));
      solo->setCheckable(true);
      addWidget(solo);

      //---------------------------------------------------
      //  Cursor Position
      //---------------------------------------------------

      QLabel* label = new QLabel;
      label->setText(tr("Cursor"));
      label->setIndent(3);
      addWidget(label);
      pos = new Awl::PosLabel;
      pos->setFixedHeight(24);
      addWidget(pos);
      if (showPitch) {
            pitch = new Awl::PitchLabel;
            pitch->setEnabled(false);
            pitch->setFixedHeight(24);
            addWidget(pitch);
            }
      else
            pitch = 0;

      //---------------------------------------------------
      //  Raster, Quant.
      //---------------------------------------------------

      QLabel* l = new QLabel(tr("Snap"));
      l->setIndent(5);
      addWidget(l);
      raster = new RasterCombo;
      addWidget(raster);

      l = new QLabel(tr("Quantize"));
      l->setIndent(5);
      addWidget(l);
      quant = new QuantCombo;
      addWidget(quant);

      raster->setFixedHeight(24);
      quant->setFixedHeight(24);

      setRaster(r);
      setQuant(q);

      //---------------------------------------------------
      //  To Menu
      //---------------------------------------------------

      addWidget(new QLabel(tr("To")));
      toList = new QComboBox;
      toList->setFixedHeight(24);
      addWidget(toList);
      toList->addItem(tr("All Events"),   RANGE_ALL);
      toList->addItem(tr("Looped Ev."),   RANGE_LOOPED);
      toList->addItem(tr("Selected Ev."), RANGE_SELECTED);
      toList->addItem(tr("Looped+Sel."),  RANGE_LOOPED | RANGE_SELECTED);

      connect(raster, SIGNAL(rasterChanged(int)), SIGNAL(rasterChanged(int)));
      connect(quant,  SIGNAL(quantChanged(int)), SIGNAL(quantChanged(int)));
      connect(toList, SIGNAL(activated(int)), SIGNAL(toChanged(int)));
      connect(solo,   SIGNAL(toggled(bool)), SIGNAL(soloChanged(bool)));
      pos->setEnabled(false);
      }

//---------------------------------------------------------
//   setApplyTo
//---------------------------------------------------------

void Toolbar1::setApplyTo(int val)
      {
	toList->setCurrentIndex(toList->findData(val));
      }

//---------------------------------------------------------
//   setPitch
//---------------------------------------------------------

void Toolbar1::setPitch(int val)
      {
      if (pitch) {
            pitch->setEnabled(val != -1);
            pitch->setPitch(val);
            }
      }

//---------------------------------------------------------
//   setInt
//---------------------------------------------------------

void Toolbar1::setInt(int val)
      {
      if (pitch) {
            pitch->setEnabled(val != -1);
            pitch->setInt(val);
            }
      }

//---------------------------------------------------------
//   setTime
//---------------------------------------------------------

void Toolbar1::setTime(const AL::Pos& val, bool enable)
      {
      pos->setValue(val, enable);
      }

//---------------------------------------------------------
//   setRaster
//---------------------------------------------------------

void Toolbar1::setRaster(int val)
      {
      raster->setRaster(val);
      }

//---------------------------------------------------------
//   setQuant
//---------------------------------------------------------

void Toolbar1::setQuant(int val)
      {
      quant->setQuant(val);
      }

//---------------------------------------------------------
//   setSolo
//---------------------------------------------------------

void Toolbar1::setSolo(bool flag)
      {
      solo->setChecked(flag);
      }

//---------------------------------------------------------
//   setPitchMode
//---------------------------------------------------------

void Toolbar1::setPitchMode(bool flag)
      {
      if (pitch)
            pitch->setPitchMode(flag);
      }
