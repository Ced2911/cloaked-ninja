/*  Pokopom - Input Plugin for PSX/PS2 Emulators
 *  Copyright (C) 2012  KrossX
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "General.h"

_Settings::_Settings()
{
	SetDefaults();
}

void _Settings::SetDefaults()
{
	SwapXO = false;
	SwapSticksEnabled = false;
	greenAnalog = false;
	defaultAnalog = false;
	isGuitar = false;
	disabled = false;
	sticksLocked = true;
	xinputPort = 0;

	stickL.SetDefaults();
	stickR.SetDefaults();

	rumble = 1.0;
	pressureRate = 10;

	for(s16 i = 0; i < 4; i++)
	{
		axisInverted[i] = false;
		axisRemap[i] = i;
		axisValue[i] = 0;
	}
}

void StickSettings::SetDefaults()
{
	b4wayDAC = false;
	DACenabled = true;
	DACthreshold = 28000;

	extThreshold = 32767.0; // 40201 real max radius
	extMult = 1.4142135623730950488016887242097; // sqrt(2)

	deadzone = 0.0;
	antiDeadzone = 0.0;
	linearity = 0.0;
}
