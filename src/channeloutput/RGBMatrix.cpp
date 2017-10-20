/*
 *   librgbmatrix handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "common.h"
#include "log.h"
#include "RGBMatrix.h"
#include "settings.h"

#define RGBMatrix_MAX_PIXELS    512 * 4
#define RGBMatrix_MAX_CHANNELS  RGBMatrix_MAX_PIXELS * 3

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
RGBMatrixOutput::RGBMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_gpio(NULL),
	m_canvas(NULL),
	m_colorOrder("RGB"),
	m_panelWidth(32),
	m_panelHeight(16),
	m_panels(0),
	m_width(0),
	m_height(0),
	m_rows(0),
	m_outputs(0),
	m_longestChain(0),
	m_invertedData(0),
	m_gpioSlowdown(1)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::RGBMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
RGBMatrixOutput::~RGBMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::~RGBMatrixOutput()\n");

	delete m_matrix;
	delete m_panelMatrix;
}

/*
 *
 */
int RGBMatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Init(JSON)\n");

	m_gpioSlowdown = getSettingInt("slowdownGPIO");

	if (!m_gpioSlowdown)
		m_gpioSlowdown = 1;

	m_panelWidth = config["panelWidth"].asInt();
	m_panelHeight = config["panelHeight"].asInt();

	if (!m_panelWidth)
		m_panelWidth = 32;

	if (!m_panelHeight)
		m_panelHeight = 16;

	m_options.rows = m_panelHeight;

	m_invertedData = config["invertedData"].asInt();
	m_colorOrder = config["colorOrder"].asString();
	m_options.led_rgb_sequence = m_colorOrder.c_str();

	m_panelMatrix =
		new PanelMatrix(m_panelWidth, m_panelHeight, 3, m_invertedData);

	if (!m_panelMatrix)
	{
		LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");

		return 0;
	}

	for (int i = 0; i < config["panels"].size(); i++)
	{
		Json::Value p = config["panels"][i];
		char orientation = 'N';
		const char *o = p["orientation"].asString().c_str();

		if (o && *o)
			orientation = o[0];

		m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
			p["panelNumber"].asInt(), orientation,
			p["xOffset"].asInt(), p["yOffset"].asInt());

		if (p["outputNumber"].asInt() > m_outputs)
			m_outputs = p["outputNumber"].asInt();

		if (p["panelNumber"].asInt() > m_longestChain)
			m_longestChain = p["panelNumber"].asInt();
	}

	// Both of these are 0-based, so bump them up by 1 for comparisons
	m_outputs++;
	m_longestChain++;

	m_options.chain_length = m_longestChain;
	m_options.parallel = m_outputs;

	m_panels = m_panelMatrix->PanelCount();

	if (config.isMember("brightness"))
		m_options.brightness = config["brightness"].asInt();
	else
		m_options.brightness = 100;

	m_options.pwm_bits = getSettingInt("pwmBits");

	if (!m_options.pwm_bits)
		m_options.pwm_bits = 8;

	if (config.isMember("wiringPinout"))
		m_options.hardware_mapping = config["wiringPinout"].asString().c_str();
	else
		m_options.hardware_mapping = "regular";

	m_options.pwm_lsb_nanoseconds = getSettingInt("pwmLSB");

	if (!m_options.pwm_lsb_nanoseconds)
		m_options.pwm_lsb_nanoseconds = 130;

	m_options.scan_mode = getSettingInt("scanMode");

	if (!m_options.scan_mode)
		m_options.scan_mode = 0;

	m_gpio = new GPIO();
	if (!m_gpio)
	{
		LogErr(VB_CHANNELOUT, "Unable to create GPIO instance\n");

		return 0;
	}

	if (!m_gpio->Init(m_gpioSlowdown))
	{
		LogErr(VB_CHANNELOUT, "GPIO->Init() failed\n");

		delete m_gpio;
		return 0;
	}

	m_rows = m_panelHeight;

	m_width  = m_panelMatrix->Width();
	m_height = m_panelMatrix->Height();

	m_channelCount = m_width * m_height * 3;

	m_canvas = new RGBMatrix(m_gpio, m_options);
	if (!m_canvas)
	{
		LogErr(VB_CHANNELOUT, "Unable to create Canvas instance\n");

		delete m_gpio;
		m_gpio = NULL;

		return 0;
	}

	RGBMatrix *rgbmatrix = reinterpret_cast<RGBMatrix*>(m_canvas);
	//rgbmatrix->SetPWMBits(8);

	m_matrix = new Matrix(m_startChannel, m_width, m_height);

	if (config.isMember("subMatrices"))
	{
		for (int i = 0; i < config["subMatrices"].size(); i++)
		{
			Json::Value sm = config["subMatrices"][i];

			m_matrix->AddSubMatrix(
				sm["enabled"].asInt(),
				sm["startChannel"].asInt() - 1,
				sm["width"].asInt(),
				sm["height"].asInt(),
				sm["xOffset"].asInt(),
				sm["yOffset"].asInt());
		}
	}

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int RGBMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Close()\n");

	m_canvas->Fill(0,0,0);

	delete m_canvas;
	m_canvas = NULL;

	delete m_gpio;
	m_gpio = NULL;

	return ChannelOutputBase::Close();
}

/*
 *
 */
void RGBMatrixOutput::PrepData(unsigned char *channelData)
{
	m_matrix->OverlaySubMatrices(channelData);
}

/*
 *
 */
int RGBMatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::RawSendData(%p)\n",
		channelData);

	unsigned char *r = NULL;
	unsigned char *g = NULL;
	unsigned char *b = NULL;

	for (int output = 0; output < m_outputs; output++)
	{
		int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

		for (int i = 0; i < panelsOnOutput; i++)
		{
			int panel = m_panelMatrix->m_outputPanels[output][i];

			int chain = (panelsOnOutput - 1) - m_panelMatrix->m_panels[panel].chain;
			for (int y = 0; y < m_panelHeight; y++)
			{
				int px = chain * m_panelWidth;
				for (int x = 0; x < m_panelWidth; x++)
				{
					r = channelData + m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3];
					g = r + 1;
					b = r + 2;

					m_canvas->SetPixel(px, y + (output * m_panelHeight), *r, *g, *b);

					px++;
				}
			}
		}
	}

	return m_channelCount;
}

/*
 *
 */
void RGBMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::DumpConfig()\n");
	LogDebug(VB_CHANNELOUT, "    panels : %d\n", m_panels);
	LogDebug(VB_CHANNELOUT, "    width  : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    height : %d\n", m_height);
}

