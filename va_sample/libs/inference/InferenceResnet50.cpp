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

#include "InferenceResnet50.h"
#include <ie_plugin_config.hpp>
#include <inference_engine.hpp>
#include <logs.h>
#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceResnet50::InferenceResnet50():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_resultSize(0)
{
}

InferenceResnet50::~InferenceResnet50()
{
}

void InferenceResnet50::SetDataPorts()
{
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
	inputInfoFirst->setPrecision(Precision::U8);
	inputInfoFirst->getInputData()->setLayout(Layout::NCHW);
    if (m_shareSurfaceWithVA)
    {
        inputInfoFirst->getPreProcess().setColorFormat(ColorFormat::NV12);
    }

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	_output->setPrecision(Precision::FP32);
}

int InferenceResnet50::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    TRACE("InferenceOV::Load()   ret %d  ", ret);
    if (ret)
    {
        ERRLOG("--load model failed!\n");
        return ret;
    }

    // ---------------------------Set outputs ------------------------------------------------------	
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    auto& _output = outputInfo.begin()->second;
    const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
    m_resultSize = outputDims[1];

    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    auto& inputInfoFirst = inputInfo.begin()->second;
    const InferenceEngine::SizeVector inputDims = inputInfoFirst->getTensorDesc().getDims();
    m_channelNum = inputDims[1];
    m_inputWidth = inputDims[3];
    m_inputHeight = inputDims[2];
    return 0;
}

void InferenceResnet50::GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc)
{
    *width = m_inputWidth;
    *height = m_inputHeight;
    *fourcc = m_shareSurfaceWithVA?0x3231564e:0x50424752; //MFX_FOURCC_RGBP
    TRACE("*width %d, *height %d, *fourcc 0x%X \n", *width, *height, *fourcc);
}

void InferenceResnet50::CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex)
{
    uint8_t *input = (uint8_t *)dst;
    TRACE("batchIndex %d, m_inputWidth %d, m_inputHeight %d, m_channelNum %d", batchIndex, m_inputWidth, m_inputHeight, m_channelNum);
    input += batchIndex * m_inputWidth * m_inputHeight * m_channelNum;
    //
    // The img in BGR format
    //cv::Mat imgMat(m_inputHeight, m_inputWidth, CV_8UC1, img);
    //std::vector<cv::Mat> inputChannels;
    //for (int i = 0; i < m_channelNum; i ++)
    //{
    //    cv::Mat channel(m_inputHeight, m_inputWidth, CV_8UC1, input + i * m_inputWidth * m_inputHeight);
    //    inputChannels.push_back(channel);
    //}
    //cv::split(imgMat, inputChannels);

    //
    // The img should already in RGBP format, if not, using the above code
    memcpy(input, img, m_channelNum * m_inputWidth * m_inputHeight);
}

int InferenceResnet50::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds)
{
    std::map<std::string, const float*>* curResults = (std::map<std::string, const float*>*) result;
    float* curResult = (float*)curResults->find(m_outputsNames[0])->second;
    TRACE("");
    for (int i = 0; i < count; i ++)
    {
        int c = -1;
        float conf = 0;
        for (int j = 0; j < m_resultSize; j ++)
        {
            if (curResult[j] > conf)
            {
                c = j;
                conf = curResult[j];
            }
        }
        VAData *data = VAData::Create(c, conf);
        data->SetID(channelIds[i], frameIds[i]);
        // one roi creates one output, just copy the roiIds
        data->SetRoiIndex(roiIds[i]);
        datas.push_back(data);
        curResult += m_resultSize;
    }
    return 0;
}

