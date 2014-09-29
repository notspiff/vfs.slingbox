/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "xbmc/libXBMC_addon.h"
#include "xbmc/threads/mutex.h"
#include <fcntl.h>
#include <map>
#include <sstream>
#include <string.h>

#include "SlingboxLib.h"

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

extern "C" {

#include "xbmc/xbmc_vfs_dll.h"
#include "xbmc/IFileTypes.h"

struct SlingContext
{
  CSlingbox sling;
  std::string hostname;
  CSlingbox::Resolution resolution;
  int video_bitrate;
  int video_framerate;
  int video_smoothing;
  int video_iframeinterval;
  int audio_bitrate;

  std::vector<int> channel_codes;
  int channel_up;
  int channel_down;

  SlingContext()
  {
    // the rest is obtained from XML
    channel_up = channel_down = 0;
  }

  void CopySettings(const SlingContext& ctx)
  {
    resolution           = ctx.resolution;
    video_bitrate        = ctx.video_bitrate;
    video_framerate      = ctx.video_framerate;
    video_smoothing      = ctx.video_smoothing;
    video_iframeinterval = ctx.video_iframeinterval;
    audio_bitrate        = ctx.audio_bitrate;
  }
};

SlingContext g_slingbox_ctx;

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return true;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  if (strcasecmp(strSetting, "resolution") == 0)
    g_slingbox_ctx.resolution = (CSlingbox::Resolution)(*((int*)value)+1);
  else if (strcasecmp(strSetting, "videobitrate") == 0)
    g_slingbox_ctx.video_bitrate = *((int*)value);
  else if (strcasecmp(strSetting, "framerate") == 0)
  {
    static const int framerates[] = {1,5,6,10,12,15,20,24,25,30};
    int r = *((int*)value);
    if (r < 0 || r > 9)
      r = 9;
    g_slingbox_ctx.video_framerate = framerates[r];
  }
  else if (strcasecmp(strSetting, "smoothing") == 0)
    g_slingbox_ctx.video_smoothing = *((int*)value);
  else if (strcasecmp(strSetting, "iframeinterval") == 0)
    g_slingbox_ctx.video_iframeinterval = *((int*)value);
  else if (strcasecmp(strSetting, "audiobitrate") == 0)
    g_slingbox_ctx.audio_bitrate = *((int*)value);

  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

static void LoadSettings(SlingContext* ctx, const std::string& hostname)
{
  // Load default settings
/*  m_sSlingboxSettings.strHostname = strHostname;
  m_sSlingboxSettings.iVideoWidth = 320;
  m_sSlingboxSettings.iVideoHeight = 240;
  m_sSlingboxSettings.iVideoResolution = (int)CSlingbox::RESOLUTION320X240;
  m_sSlingboxSettings.iVideoBitrate = 704;
  m_sSlingboxSettings.iVideoFramerate = 30;
  m_sSlingboxSettings.iVideoSmoothing = 50;
  m_sSlingboxSettings.iAudioBitrate = 64;
  m_sSlingboxSettings.iIFrameInterval = 10;
  m_sSlingboxSettings.uiCodeChannelUp = 0;
  m_sSlingboxSettings.uiCodeChannelDown = 0;
  for (unsigned int i = 0; i < 10; i++)
    m_sSlingboxSettings.uiCodeNumber[i] = 0;

  // Check if a SlingboxSettings.xml file exists
  CStdString slingboxXMLFile = CProfilesManager::Get().GetUserDataItem("SlingboxSettings.xml");
  if (!CFile::Exists(slingboxXMLFile))
  {
    CLog::Log(LOGNOTICE, "No SlingboxSettings.xml file (%s) found - using default settings",
      slingboxXMLFile.c_str());
    return;
  }

  // Load the XML file
  CXBMCTinyXML slingboxXML;
  if (!slingboxXML.LoadFile(slingboxXMLFile))
  {
    CLog::Log(LOGERROR, "%s - Error loading %s - line %d\n%s", __FUNCTION__, 
      slingboxXMLFile.c_str(), slingboxXML.ErrorRow(), slingboxXML.ErrorDesc());
    return;
  }

  // Check to make sure layout is correct
  TiXmlElement * pRootElement = slingboxXML.RootElement();
  if (!pRootElement || strcmpi(pRootElement->Value(), "slingboxsettings") != 0)
  {
    CLog::Log(LOGERROR, "%s - Error loading %s - no <slingboxsettings> node found",
      __FUNCTION__, slingboxXMLFile.c_str());
    return;
  }

  // Success so far
  CLog::Log(LOGNOTICE, "Loaded SlingboxSettings.xml from %s", slingboxXMLFile.c_str());

  // Search for the first settings that specify no hostname or match our hostname
  TiXmlElement *pElement;
  for (pElement = pRootElement->FirstChildElement("slingbox"); pElement;
    pElement = pElement->NextSiblingElement("slingbox"))
  {
    if (pElement->Attribute("hostname") == NULL ||      
      StringUtils::EqualsNoCase(m_sSlingboxSettings.strHostname, pElement->Attribute("hostname")))
    {
      // Load setting values
      XMLUtils::GetInt(pElement, "width", m_sSlingboxSettings.iVideoWidth, 0, 640);
      XMLUtils::GetInt(pElement, "height", m_sSlingboxSettings.iVideoHeight, 0, 480);
      XMLUtils::GetInt(pElement, "videobitrate", m_sSlingboxSettings.iVideoBitrate, 50, 8000);
      XMLUtils::GetInt(pElement, "framerate", m_sSlingboxSettings.iVideoFramerate, 1, 30);
      XMLUtils::GetInt(pElement, "smoothing", m_sSlingboxSettings.iVideoSmoothing, 0, 100);
      XMLUtils::GetInt(pElement, "audiobitrate", m_sSlingboxSettings.iAudioBitrate, 16, 96);
      XMLUtils::GetInt(pElement, "iframeinterval", m_sSlingboxSettings.iIFrameInterval, 1, 30);

      // Load any button code values
      TiXmlElement * pCodes = pElement->FirstChildElement("buttons");
      if (pCodes)
      {
        XMLUtils::GetHex(pCodes, "channelup", m_sSlingboxSettings.uiCodeChannelUp);
        XMLUtils::GetHex(pCodes, "channeldown", m_sSlingboxSettings.uiCodeChannelDown);
        XMLUtils::GetHex(pCodes, "zero", m_sSlingboxSettings.uiCodeNumber[0]);
        XMLUtils::GetHex(pCodes, "one", m_sSlingboxSettings.uiCodeNumber[1]);
        XMLUtils::GetHex(pCodes, "two", m_sSlingboxSettings.uiCodeNumber[2]);
        XMLUtils::GetHex(pCodes, "three", m_sSlingboxSettings.uiCodeNumber[3]);
        XMLUtils::GetHex(pCodes, "four", m_sSlingboxSettings.uiCodeNumber[4]);
        XMLUtils::GetHex(pCodes, "five", m_sSlingboxSettings.uiCodeNumber[5]);
        XMLUtils::GetHex(pCodes, "six", m_sSlingboxSettings.uiCodeNumber[6]);
        XMLUtils::GetHex(pCodes, "seven", m_sSlingboxSettings.uiCodeNumber[7]);
        XMLUtils::GetHex(pCodes, "eight", m_sSlingboxSettings.uiCodeNumber[8]);
        XMLUtils::GetHex(pCodes, "nine", m_sSlingboxSettings.uiCodeNumber[9]);
      }

      break;
    }
  }

  // Prepare our resolution enum mapping array
  const struct
  {
    unsigned int uiWidth;
    unsigned int uiHeight;
    CSlingbox::Resolution eEnum;
  } m_resolutionMap[11] = {
    {0, 0, CSlingbox::NOVIDEO},
    {128, 96, CSlingbox::RESOLUTION128X96},
    {160, 120, CSlingbox::RESOLUTION160X120},
    {176, 120, CSlingbox::RESOLUTION176X120},
    {224, 176, CSlingbox::RESOLUTION224X176},
    {256, 192, CSlingbox::RESOLUTION256X192},
    {320, 240, CSlingbox::RESOLUTION320X240},
    {352, 240, CSlingbox::RESOLUTION352X240},
    {320, 480, CSlingbox::RESOLUTION320X480},
    {640, 240, CSlingbox::RESOLUTION640X240},
    {640, 480, CSlingbox::RESOLUTION640X480}
  };

  // See if the specified resolution matches something in our mapping array and
  // setup things accordingly
  for (unsigned int i = 0; i < 11; i++)
  {
    if (m_sSlingboxSettings.iVideoWidth == (int)m_resolutionMap[i].uiWidth &&
      m_sSlingboxSettings.iVideoHeight == (int)m_resolutionMap[i].uiHeight)
    {
      m_sSlingboxSettings.iVideoResolution = (int)m_resolutionMap[i].eEnum;
      return;
    }
  }

  // If it didn't match anything setup safe defaults
  CLog::Log(LOGERROR, "%s - Defaulting to 320x240 resolution due to invalid "
    "resolution specified in SlingboxSettings.xml for Slingbox: %s",
    __FUNCTION__, m_sSlingboxSettings.strHostname.c_str());
  m_sSlingboxSettings.iVideoWidth = 320;
  m_sSlingboxSettings.iVideoHeight = 240;
  m_sSlingboxSettings.iVideoResolution = (int)CSlingbox::RESOLUTION320X240;
  */
}

void* Open(VFSURL* url)
{
  // Setup the IP/hostname and port (setup default port if none specified)
  if (url->port == 0)
    url->port = 5001;

  SlingContext* result = new SlingContext;
  result->CopySettings(g_slingbox_ctx);

  result->sling.SetAddress(url->hostname, url->port);

  // Prepare to connect to the Slingbox
  bool bAdmin;
  if (strcasecmp(url->username, "administrator") == 0)
    bAdmin = true;
  else if (strcasecmp(url->username, "viewer") == 0)
    bAdmin = false;
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Invalid or no username specified for Slingbox: %s",
              __FUNCTION__, url->hostname);

    delete result;
    return NULL;
  }

  // Connect to the Slingbox
  if (result->sling.Connect(bAdmin, url->password))
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully connected to Slingbox: %s",
              __FUNCTION__, url->hostname);
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error connecting to Slingbox: %s",
              __FUNCTION__, url->hostname);
    delete result;
    return NULL;
  }

  // Initialize the stream
  if (result->sling.InitializeStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully initialized stream on Slingbox: %s",
              __FUNCTION__, url->hostname);
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error initializing stream on Slingbox: %s",
              __FUNCTION__, url->hostname);
    delete result;
    return NULL;
  }

  std::string filewithoutpath(url->filename);
  size_t pos = filewithoutpath.rfind("/");
  filewithoutpath.erase(filewithoutpath.begin(), filewithoutpath.begin()+pos);

  // Set correct input
  if (!filewithoutpath.empty())
  {
    if (result->sling.SetInput(atoi(filewithoutpath.c_str())))
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully requested change to input %i on Slingbox: %s",
                __FUNCTION__, atoi(filewithoutpath.c_str()), url->hostname);
    else
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error requesting change to input %i on Slingbox: %s",
                __FUNCTION__, atoi(filewithoutpath.c_str()), url->hostname);
  }

  // Load the video settings
  LoadSettings(result, url->hostname);

  // Setup video options  
  if (result->sling.StreamSettings(result->resolution, result->video_bitrate,
                                   result->video_framerate, result->video_smoothing,
                                   result->audio_bitrate, result->video_iframeinterval))
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully set stream options (resolution: %i; "
                                "video bitrate: %i kbit/s; fps: %i; smoothing: %i%%;"
                                "audio bitrate %i kbit/s; I frame interval: %i) on Slingbox: %s",
                                __FUNCTION__, result->resolution, result->video_bitrate, 
                                result->video_framerate, result->video_smoothing, 
                                result->audio_bitrate, result->video_iframeinterval, url->hostname);
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error setting stream options on Slingbox: %s",
              __FUNCTION__, url->hostname);
  }

  // Start the stream
  if (result->sling.StartStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully started stream on Slingbox: %s",
              __FUNCTION__, url->hostname);
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error starting stream on Slingbox: %s",
              __FUNCTION__, url->hostname);
    delete result;
    return NULL;
  }

  // Check for correct input
  if (!filewithoutpath.empty())
  {
    if (result->sling.GetInput() == -1)
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Unable to confirm change to input %i on Slingbox: %s",
                __FUNCTION__, atoi(filewithoutpath.c_str()), url->hostname);
    else if (result->sling.GetInput() == atoi(filewithoutpath.c_str()))
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Comfirmed change to input %i on Slingbox: %s",
                __FUNCTION__, atoi(filewithoutpath.c_str()), url->hostname);
    else
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error changing to input %i on Slingbox: %s",
                __FUNCTION__, atoi(filewithoutpath.c_str()), url->hostname);
  }

  result->hostname = url->hostname;
  return result;
}

unsigned int Read(void* context, void* lpBuf, int64_t uiBufSize)
{
  SlingContext* ctx = (SlingContext*)context;

  // Read the data and check for any errors
  int iRead = ctx->sling.ReadStream(lpBuf, (unsigned int)uiBufSize);
  if (iRead < 0)
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error reading stream from Slingbox: %s", __FUNCTION__,
              ctx->hostname.c_str());
    return 0;
  }

  return iRead;
}

bool Close(void* context)
{
  SlingContext* ctx = (SlingContext*)context;

  // Stop the stream
  if (ctx->sling.StopStream())
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully stopped stream on Slingbox: %s", __FUNCTION__,
              ctx->hostname.c_str());
  else
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error stopping stream on Slingbox: %s", __FUNCTION__,
              ctx->hostname.c_str());

  // Disconnect from the Slingbox
  if (ctx->sling.Disconnect())
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully disconnected from Slingbox: %s", __FUNCTION__,
              ctx->hostname.c_str());
  else
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error disconnecting from Slingbox: %s", __FUNCTION__,
              ctx->hostname.c_str());

  delete ctx;
}

int64_t GetLength(void* context)
{
  return -1;
}

//*********************************************************************************************
int64_t GetPosition(void* context)
{
  return -1;
}


int64_t Seek(void* context, int64_t iFilePosition, int iWhence)
{
  return -1;
}

bool Exists(VFSURL* url)
{
  return false;
}

int Stat(VFSURL* url, struct __stat64* buffer)
{
  return -1;
}

int IoControl(void* context, XFILE::EIoControl request, void* param)
{
  if(request == XFILE::IOCTRL_SEEK_POSSIBLE)
    return 1;

  return -1;
}

void ClearOutIdle()
{
}

void DisconnectAll()
{
}

bool DirectoryExists(VFSURL* url)
{
  return false;
}

void* GetDirectory(VFSURL* url, VFSDirEntry** items,
                   int* num_items, VFSCallbacks* callbacks)
{
  *items = new VFSDirEntry;
  items[0]->path = strdup(url->url);
  items[0]->label = XBMC->GetLocalizedString(30006);
  items[0]->title = NULL;
  items[0]->properties = new VFSProperty;
  items[0]->properties->name = strdup("propmisusepreformatted");
  items[0]->properties->val = strdup("true");
  items[0]->num_props = 1;
  *num_items = 1;

  return *items;
}

void FreeDirectory(void* items)
{
  VFSDirEntry* entry = (VFSDirEntry*)items;
  free(entry->path);
  XBMC->FreeString(entry->label);
  free(entry->properties->name);
  free(entry->properties->val);

  delete entry->properties;
  delete entry;
}

bool CreateDirectory(VFSURL* url)
{
  return false;
}

bool RemoveDirectory(VFSURL* url)
{
  return false;
}

int Truncate(void* context, int64_t size)
{
  return -1;
}

int Write(void* context, const void* lpBuf, int64_t uiBufSize)
{
  return -1;
}

bool Delete(VFSURL* url)
{
  return false;
}

bool Rename(VFSURL* url, VFSURL* url2)
{
  return false;
}

void* OpenForWrite(VFSURL* url, bool bOverWrite)
{ 
  return NULL;
}

void* ContainsFiles(VFSURL* url, VFSDirEntry** items, int* num_items, char* rootpath)
{
  return NULL;
}

int GetStartTime(void* ctx)
{
  return 0;
}

int GetTotalTime(void* ctx)
{
  return 0;
}

bool NextChannel(void* context, bool preview)
{
  SlingContext* ctx = (SlingContext*)context;

   // Prepare variables
  bool bSuccess = true;
  int iPrevChannel = ctx->sling.GetChannel();

  // Stop the stream
  if (ctx->sling.StopStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully stopped stream before channel change request on "
                                "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error stopping stream before channel change request on "
                                "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  // Figure out which method to use
  if (ctx->channel_up == 0)
  {
    // Change the channel
    if (ctx->sling.ChannelUp())
    {
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully requested channel change on Slingbox: %s",
                __FUNCTION__, ctx->hostname.c_str());

      if (ctx->sling.GetChannel() == -1)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Unable to confirm channel change on Slingbox: %s",
                  __FUNCTION__, ctx->hostname.c_str());
      }
      else if (ctx->sling.GetChannel() != iPrevChannel)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Confirmed change to channel %i on Slingbox: %s",
                  __FUNCTION__, ctx->sling.GetChannel(), ctx->hostname.c_str());
      }
      else
      {
        XBMC->Log(ADDON::LOG_ERROR, "%s - Error changing channel on Slingbox: %s",
                  __FUNCTION__, ctx->hostname.c_str());
        bSuccess = false;
      }
    }
    else
    {
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error requesting channel change on Slingbox: %s",
                __FUNCTION__, ctx->hostname.c_str());
      bSuccess = false;
    }
  }
  else
  {
    // Change the channel using IR command
    if (ctx->sling.SendIRCommand(ctx->channel_up))
    {
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully sent IR command (code: 0x%.2X) from "
                "Slingbox: %s", __FUNCTION__, ctx->channel_up, ctx->hostname.c_str());
    }
    else
    {
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error sending IR command (code: 0x%.2X) from "
                "Slingbox: %s", __FUNCTION__, ctx->channel_up, ctx->hostname.c_str());
      bSuccess = false;
    }
  }

  // Start the stream again
  if (ctx->sling.StartStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully started stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error starting stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  return bSuccess;
}

bool PrevChannel(void* context, bool preview)
{
  SlingContext* ctx = (SlingContext*)context;

   // Prepare variables
  bool bSuccess = true;
  int iPrevChannel = ctx->sling.GetChannel();

  // Stop the stream
  if (ctx->sling.StopStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully stopped stream before channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error stopping stream before channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  // Figure out which method to use
  if (ctx->channel_down == 0)
  {
    // Change the channel
    if (ctx->sling.ChannelDown())
    {
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully requested channel change on Slingbox: %s",
                __FUNCTION__, ctx->hostname.c_str());

      if (ctx->sling.GetChannel() == -1)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Unable to confirm channel change on Slingbox: %s",
                  __FUNCTION__, ctx->hostname.c_str());
      }
      else if (ctx->sling.GetChannel() != iPrevChannel)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Confirmed change to channel %i on Slingbox: %s",
                  __FUNCTION__, ctx->sling.GetChannel(), ctx->hostname.c_str());
      }
      else
      {
        XBMC->Log(ADDON::LOG_ERROR, "%s - Error changing channel on Slingbox: %s",
                  __FUNCTION__, ctx->hostname.c_str());
        bSuccess = false;
      }
    }
    else
    {
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error requesting channel change on Slingbox: %s",
                __FUNCTION__, ctx->hostname.c_str());
      bSuccess = false;
    }
  }
  else
  {
    // Change the channel using IR command
    if (ctx->sling.SendIRCommand(ctx->channel_down))
    {
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully sent IR command (code: 0x%.2X) from "
                "Slingbox: %s", __FUNCTION__, ctx->channel_down, ctx->hostname.c_str());
    }
    else
    {
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error sending IR command (code: 0x%.2X) from "
                "Slingbox: %s", __FUNCTION__, ctx->channel_down, ctx->hostname.c_str());
      bSuccess = false;
    }
  }    

  // Start the stream again
  if (ctx->sling.StartStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully started stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error starting Slingbox stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  return bSuccess;
}

bool SelectChannel(void* context, unsigned int uiChannel)
{
  SlingContext* ctx = (SlingContext*)context;
  // Check if a channel change is required
  if (ctx->sling.GetChannel() == (int)uiChannel)
    return false;

  // Prepare variables
  bool bSuccess = true;

  // Stop the stream
  if (ctx->sling.StopStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully stopped stream before channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error stopping stream before channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  // Figure out which method to use
  unsigned int uiButtonsWithCode = 0;
  for (unsigned int i = 0; i < ctx->channel_codes.size(); i++)
  {
    if (ctx->channel_codes[i] != 0)
      uiButtonsWithCode++;
  }
  if (uiButtonsWithCode == 0)
  {
    // Change the channel
    if (ctx->sling.SetChannel(uiChannel))
    {
      XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully requested change to channel %i on Slingbox: %s",
                __FUNCTION__, uiChannel, ctx->hostname.c_str());

      if (ctx->sling.GetChannel() == -1)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Unable to confirm change to channel %i on Slingbox: %s",
                  __FUNCTION__, uiChannel, ctx->hostname.c_str());
      }
      else if (ctx->sling.GetChannel() == (int)uiChannel)
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Confirmed change to channel %i on Slingbox: %s",
                  __FUNCTION__, uiChannel, ctx->hostname.c_str());
      }
      else
      {
        XBMC->Log(ADDON::LOG_ERROR, "%s - Error changing to channel %i on Slingbox: %s",
                  __FUNCTION__, uiChannel, ctx->hostname.c_str());
        bSuccess = false;
      }
    }
    else
    {
      XBMC->Log(ADDON::LOG_ERROR, "%s - Error requesting change to channel %i on Slingbox: %s",
                __FUNCTION__, uiChannel, ctx->hostname.c_str());
      bSuccess = false;
    }
  }
  else if (uiButtonsWithCode == 10)
  {
    // Prepare variables
    char temp[16];
    sprintf(temp,"%u", uiChannel);
    std::string strDigits = temp;
    size_t uiNumberOfDigits = strDigits.size();

    // Change the channel using IR commands
    for (size_t i = 0; i < uiNumberOfDigits; i++)
    {
      if (ctx->sling.SendIRCommand(ctx->channel_codes[strDigits[i] - '0']))
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully sent IR command (code: 0x%.2X) from "
                  "Slingbox: %s", __FUNCTION__, ctx->channel_codes[strDigits[i] - '0'],
                  ctx->hostname.c_str());
      }
      else
      {
        XBMC->Log(ADDON::LOG_DEBUG, "%s - Error sending IR command (code: 0x%.2X) from "
                  "Slingbox: %s", __FUNCTION__, ctx->channel_codes[strDigits[i] - '0'],
                  ctx->hostname.c_str());
        bSuccess = false;
      }
    }
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error requesting change to channel %i on Slingbox due to one or more "
              "missing button codes from advancedsettings.xml for Slingbox: %s",
              __FUNCTION__, uiChannel, ctx->hostname.c_str());
    bSuccess = false;
  }

  // Start the stream again
  if (ctx->sling.StartStream())
  {
    XBMC->Log(ADDON::LOG_DEBUG, "%s - Successfully started stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
  }
  else
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Error starting stream after channel change request on "
              "Slingbox: %s", __FUNCTION__, ctx->hostname.c_str());
    bSuccess = false;
  }

  return bSuccess;
}

bool UpdateItem(void* context)
{
  return false;
}

int GetChunkSize(void* context)
{
  return 0;
}

}
