/*
* Copyright (c) 2019, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include <EncodeThreadBlock.h>
#include "common.h"
#include <iostream>
#include <queue>
#include <map>
#include <fstream>

#include "MfxSessionMgr.h"

void DumpNV12(char* y, char* uv, int w, int h)
{
    static int index = 0;
    char filename[256] = {};
    snprintf(filename, 256, "out.%04d.nv12", index);
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    file.write(y, w*h);
    file.write(uv, w*h/2);
    file.close();
    index++;
}

EncodeThreadBlock::EncodeThreadBlock(uint32_t channel, VAEncodeType type):
    m_channel(channel),
    m_mfxSession (nullptr),
    m_mfxAllocator(nullptr),
    m_mfxEncode(nullptr),
    m_encodeSurfaces(nullptr),
    m_encodeSurfNum(0),
    m_asyncDepth(1),
    m_inputFormat(MFX_FOURCC_NV12),
    m_inputWidth(0),
    m_inputHeight(0),
    m_encodeOutDump(true),
    m_debugPrintCounter(0),
    m_fp(nullptr),
    m_encodeType(type),
    m_outputRef(1)
{
    memset(&m_encParams, 0, sizeof(m_encParams));
    memset(m_outBuffers, 0, sizeof(m_outBuffers));
    memset(m_outRefs, 0, sizeof(m_outRefs));
}

EncodeThreadBlock::EncodeThreadBlock(uint32_t channel, VAEncodeType type, MFXVideoSession *mfxSession, mfxFrameAllocator *allocator):
    EncodeThreadBlock(channel, type)
{
    m_mfxSession = mfxSession;
    m_mfxAllocator = allocator;
}

EncodeThreadBlock::~EncodeThreadBlock()
{
    if (m_fp)
    {
        std::fclose(m_fp);
    }
    MfxSessionMgr::getInstance().Clear(m_channel);
}

int EncodeThreadBlock::PrepareInternal()
{
    mfxStatus sts;

    if (m_mfxSession == nullptr)
    {
        m_mfxSession = MfxSessionMgr::getInstance().GetSession(m_channel);
        if (m_mfxSession == nullptr)
        {
            ERRLOG("Fail to get MSDK session!\n");
            return -1;
        }
    }
    if (m_mfxAllocator == nullptr)
    {
        m_mfxAllocator = MfxSessionMgr::getInstance().GetAllocator(m_channel);
        if (m_mfxAllocator == nullptr)
        {
            ERRLOG("Fail to get MSDK allocator!\n");
            return -1;
        }
    }

    m_mfxEncode = new MFXVideoENCODE(*m_mfxSession);

    memset(&m_encParams, 0, sizeof(m_encParams));
    
    m_encParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    switch(m_encodeType)
    {
        case VAEncodeJpeg:
            PrepareJpeg();
            break;
        case VAEncodeAvc:
            PrepareAvc();
            break;
        default:
            return -1;
    }

    // frame info parameters
    m_encParams.mfx.FrameInfo.FourCC       = m_inputFormat;
    m_encParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_encParams.mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;

    m_encParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(m_inputWidth);
    m_encParams.mfx.FrameInfo.Height = MSDK_ALIGN16(m_inputHeight);

    m_encParams.mfx.FrameInfo.CropX = 0; //VPPParams.vpp.Out.CropX;
    m_encParams.mfx.FrameInfo.CropY = 0; //VPPParams.vpp.Out.CropY;
    m_encParams.mfx.FrameInfo.CropW = m_inputWidth;
    m_encParams.mfx.FrameInfo.CropH = m_inputHeight;

    m_encParams.AsyncDepth = m_asyncDepth;

    //sts = m_mfxEncode->Query(&m_encParams, &m_encParams);
    //MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxFrameAllocRequest EncRequest = { 0 };
    sts = m_mfxEncode->QueryIOSurf(&m_encParams, &EncRequest);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    EncRequest.NumFrameSuggested = EncRequest.NumFrameSuggested + m_encParams.AsyncDepth;

    // Prepare media sdk bit stream buffer
    // - Arbitrary buffer size for this example        
    memset(&m_mfxBS, 0, sizeof(m_mfxBS));
    m_mfxBS.MaxLength = m_encParams.mfx.FrameInfo.Width * m_encParams.mfx.FrameInfo.Height * 4;

    // allocate output buffers
    for (int i = 0; i < m_bufferNum; i++)
    {
        m_outBuffers[i] = new uint8_t[m_mfxBS.MaxLength];
    }

    // Prepare media sdk decoder parameters
    //ReadBitStreamData();

    // memory allocation
    mfxFrameAllocResponse EncResponse = { 0 };
    sts = m_mfxAllocator->Alloc(m_mfxAllocator->pthis, &EncRequest, &EncResponse);
    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
        return sts;
    }
    m_encodeSurfNum = EncResponse.NumFrameActual;
    // this surface will be used when decodes encoded stream
    m_encodeSurfaces = new mfxFrameSurface1 * [m_encodeSurfNum];
    if(!m_encodeSurfaces)
    {
        MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
        return -1;
    }

    for (int i = 0; i < m_encodeSurfNum; i++)
    {
        m_encodeSurfaces[i] = new mfxFrameSurface1;
        memset(m_encodeSurfaces[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_encodeSurfaces[i]->Info), &(m_encParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        // external allocator used - provide just MemIds
        m_encodeSurfaces[i]->Data.MemId = EncResponse.mids[i];
    }

    // Initialize the Media SDK encoder
    std::cout << "\t. Init Intel Media SDK Encoder" << std::endl;

    sts = m_mfxEncode->Init(&m_encParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
    }

    printf("\t\t.. Support Jpeg Encoder and use video memory.\n");

    return 0;
}

void EncodeThreadBlock::PrepareJpeg()
{
    m_encParams.mfx.CodecId = MFX_CODEC_JPEG;
    
    m_encParams.mfx.Interleaved = 1;
    m_encParams.mfx.Quality = 95;
    m_encParams.mfx.RestartInterval = 0;
    m_encParams.mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
    m_encParams.mfx.NumThread = 1;
    memset(&m_encParams.mfx.reserved5,0,sizeof(m_encParams.mfx.reserved5));
}

void EncodeThreadBlock::PrepareAvc()
{
    m_encParams.mfx.CodecId = MFX_CODEC_AVC;

    m_encParams.mfx.TargetUsage = MFX_TARGETUSAGE_4;
    m_encParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_encParams.mfx.QPI = 25;
    m_encParams.mfx.QPP = 25;
    m_encParams.mfx.QPB = 25;
    m_encParams.mfx.FrameInfo.FrameRateExtN = 30;
    m_encParams.mfx.FrameInfo.FrameRateExtD = 1;
}

bool EncodeThreadBlock::CanBeProcessed(VAData *data)
{
    if (!(data->Type() == USER_SURFACE || data->Type() == MFX_SURFACE))
    {
        return false;
    }
    uint32_t w, h, p, format;
    data->GetSurfaceInfo(&w, &h, &p, &format);

    bool bResult = (w == m_encParams.mfx.FrameInfo.Width &&
                    h == m_encParams.mfx.FrameInfo.Height &&
                    format == m_inputFormat);

    return bResult;
}

int EncodeThreadBlock::FindFreeOutput()
{
    for (int i = 0; i < m_bufferNum; i ++)
    {
        if (m_outRefs[i] <= 0)
        {
            return i;
        }
    }
    // no free output found
    return -1;
}

int EncodeThreadBlock::GetFreeSurface(mfxFrameSurface1 **surfaces, uint32_t count)
{
    if (surfaces)
    {
        for (uint32_t i = 0; i < count; i ++)
        {
            if (surfaces[i]->Data.Locked == 0)
            {
                return i;
            }
        }
    }
    return MFX_ERR_NOT_FOUND;
}

int EncodeThreadBlock::Loop()
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bNeedMore = false;
    mfxSyncPoint syncpEnc;
    mfxSyncPoint syncpDec;
    mfxSyncPoint syncpVPP;
    int nIndexDec = 0;
    int nIndexVpIn = 0;
    int nIndexVpOut = 0;
    uint32_t nDecoded = 0;
    std::queue<uint64_t> hasOutputs;
    std::map<uint64_t, VADataPacket *> recordedPackets;
    VAData *data = nullptr;

    while (!m_stop)
    {
        VADataPacket *InPacket = AcquireInput();
        VADataPacket *OutPacket = DequeueOutput();

        if (!InPacket || !OutPacket)
            break;

        // get all the inputs
        std::list<VAData *>vpOuts;
        for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
        {
            data = *ite;
            if (CanBeProcessed(data))
            {
                vpOuts.push_back(data);
            }
            else
            {
                OutPacket->push_back(data);
            }
        }
        ReleaseInput(InPacket);

        while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts))
        {
            if (vpOuts.empty())
            {
                printf("EncodeThreadBlock: EMPTY input!\n" );
                // This is not fatal error... lets try to continue
                sts = MFX_ERR_MORE_DATA;
                break;
            }
            VAData *input = vpOuts.front();
            mfxFrameSurface1 *pInputSurface = nullptr; 
            if(input->Type() == USER_SURFACE) 
            {
                int rawYuvIndex = GetFreeSurface(m_encodeSurfaces, m_encodeSurfNum);

                uint8_t *src = input->GetSurfacePointer();
                uint32_t w, h, p, format;
                input->GetSurfaceInfo(&w, &h, &p, &format);
                pInputSurface = m_encodeSurfaces[rawYuvIndex];

                m_mfxAllocator->Lock(m_mfxAllocator->pthis, pInputSurface->Data.MemId, &(pInputSurface->Data));

                mfxFrameInfo *pInfo = &pInputSurface->Info;
                mfxFrameData *pData = &pInputSurface->Data;
                memcpy(pData->Y, src, w*h);
                memcpy(pData->UV, src + w*h, w*h/2);
                //DumpNV12((char*)pData->Y, (char*)pData->UV, w, h);

                m_mfxAllocator->Unlock(m_mfxAllocator->pthis, pInputSurface->Data.MemId, &(pInputSurface->Data));
            } 
            else 
            {
                pInputSurface = input->GetMfxSurface();
            }

            int index = -1;
            while (index == -1)
            {
                index = FindFreeOutput();
                if (index == -1)
                    usleep(10000);
            }
            m_mfxBS.Data = m_outBuffers[index];
            sts = m_mfxEncode->EncodeFrameAsync(NULL, pInputSurface, &m_mfxBS, &syncpEnc);

            if (MFX_ERR_NONE < sts && !syncpEnc) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                {
                    std::cout << std::endl << "> warning: MFX_WRN_DEVICE_BUSY" << std::endl;
                    usleep(1000); // wait if device is busy
                }
            }
            else if (MFX_ERR_NONE < sts && syncpEnc)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
            }
            else if (MFX_ERR_MORE_DATA == sts)
            {
                vpOuts.pop_front();
                break;
            }

            if (MFX_ERR_NONE == sts)
            {
                sts = m_mfxSession->SyncOperation(syncpEnc, 60000); /* Encoder's Sync */
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                if (m_encodeOutDump)
                {
                    DumpOutput(m_mfxBS.Data, m_mfxBS.DataLength, input->ChannelIndex(), input->FrameIndex());
                } // if (m_encodeOutDump)

                if (m_outputRef > 0)
                {
                    VAData *output = VAData::Create(m_mfxBS.Data, 0, m_mfxBS.DataLength);
                    output->SetExternalRef(&m_outRefs[index]);
                    output->SetRef(m_outputRef);
                    output->SetID(input->ChannelIndex(), input->FrameIndex());
                    OutPacket->push_back(output);
                }

                input->DeRef(OutPacket);
                m_mfxBS.DataLength = 0;
                m_mfxBS.Data = nullptr;
                vpOuts.pop_front();
                break;
            }
        }
        // NO output for a while
        EnqueueOutput(OutPacket);
    } // while(true)

    return 0;
}

void EncodeThreadBlock::DumpOutput(uint8_t *data, uint32_t length, uint8_t channel, uint8_t frame)
{
    switch (m_encodeType)
    {
        case VAEncodeJpeg:
        {
            std::string file_dump_name = "va_sample_jpeg_debug_" + std::to_string(channel)
                     + "." + std::to_string(frame) + ".jpeg";
            if(FILE *m_dump_bitstream_1 = std::fopen(file_dump_name.c_str(), "wb"))
            {
                std::fwrite(data, sizeof(char), length, m_dump_bitstream_1);
                std::fclose(m_dump_bitstream_1);
            }
            break;
        }
        case VAEncodeAvc:
        {
            if (m_fp == nullptr)
            {
                std::string file_dump_name = "va_sample_avc_debug_" + std::to_string(channel) + ".264";
                m_fp = std::fopen(file_dump_name.c_str(), "wb");
                if (m_fp == nullptr)
                {
                    ERRLOG("Fail to open %s\n", file_dump_name.c_str());
                    return;
                }
            }
            std::fwrite(data, sizeof(char), length, m_fp);
            std::fflush(m_fp);
            break;
        }
        default:
            break;
    }
}

