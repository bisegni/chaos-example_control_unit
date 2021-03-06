/*
 *	RTWorkerCU.cpp
 *	!CHOAS
 *	Created by Bisegni Claudio.
 *
 *    	Copyright 2012 INFN, National Institute of Nuclear Physics
 *
 *    	Licensed under the Apache License, Version 2.0 (the "License");
 *    	you may not use this file except in compliance with the License.
 *    	You may obtain a copy of the License at
 *
 *    	http://www.apache.org/licenses/LICENSE-2.0
 *
 *    	Unless required by applicable law or agreed to in writing, software
 *    	distributed under the License is distributed on an "AS IS" BASIS,
 *    	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    	See the License for the specific language governing permissions and
 *    	limitations under the License.
 */
#include <boost/thread.hpp>

#include "RTWorkerCU.h"

#include <chaos/common/global.h>
#include <chaos/common/chaos_constants.h>
#include <chaos/common/bson/bson.h>
#include <chaos/common/bson/util/hex.h>
#include <chaos/common/action/ActionDescriptor.h>
#include <chaos/cu_toolkit/ControlManager/handler/DSInt32Handler.h>
#include <chaos/cu_toolkit/ControlManager/handler/DSDoubleHandler.h>

#include <cmath>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <stdlib.h>
using namespace chaos;

#define SIMULATED_DEVICE_ID     "SIN_DEVICE"
#define DS_ELEMENT_1            "sinOutput"
#define DS_ELEMENT_2            "sinAltitude"
#define DS_ELEMENT_3            "sinPahseTime"

#define TEST_BUFFER_DIM         100
#define CU_DELAY_FROM_TASKS     1000000 //1Sec
#define ACTION_TWO_PARAM_NAME   "actionTestTwo_paramName"

/*
 Construct a new CU with an identifier
 */
RTWorkerCU::RTWorkerCU(string &customDeviceID):rng((const uint_fast32_t) time(0) ),one_to_hundred( -100, 100 ),randInt(rng, one_to_hundred){
    _deviceID = customDeviceID;
    numberOfResponse = 0;
}

/*
 Destructor a new CU with an identifier
 */
RTWorkerCU::~RTWorkerCU() {
    
}

/*
 Return the default configuration
 */
void RTWorkerCU::unitDefineActionAndDataset() throw(CException) {
    //cuSetup.addStringValue(CUDefinitionKey::CS_CM_CU_DESCRIPTION, "This is a beautifull CU");
    
    
    //set the default delay for the CU
    setDefaultScheduleDelay(CU_DELAY_FROM_TASKS);
    
    //add managed device di
    setDeviceID(_deviceID);
    
    
    //add custom action
    AbstActionDescShrPtr
    actionDescription = addActionDescritionInstance<RTWorkerCU>(this,
                                                                &RTWorkerCU::actionTestOne,
                                                                "actionTestOne",
                                                                "comandTestOne this action will do some beautifull things!");
    
    actionDescription = addActionDescritionInstance<RTWorkerCU>(this,
                                                                &RTWorkerCU::resetStatistic,
                                                                "resetStatistic",
                                                                "resetStatistic this action will reset  all cu statistic!");
    
    actionDescription = addActionDescritionInstance<RTWorkerCU>(this,
                                                                &RTWorkerCU::actionTestTwo,
                                                                "actionTestTwo",
                                                                "comandTestTwo, this action will do some beautifull things!");
    
    //add param to second action
    actionDescription->addParam(ACTION_TWO_PARAM_NAME,
                                DataType::TYPE_INT32,
                                "integer 32bit action param description for testing purpose");
    
    
    //setup the dataset
    addAttributeToDataSet("sinWave",
                          "The sin waveform",
                          DataType::TYPE_BYTEARRAY,
                          DataType::Output,
                          10000);
    
    addInputInt32AttributeToDataSet<RTWorkerCU>("points",
                                                "The number of point that compose the wave",
                                                this,
                                                &RTWorkerCU::setWavePoint);
    
    addInputDoubleAttributeToDataSet<RTWorkerCU>("frequency",
                                                 "The frequency of the wave [1-10Mhz]",
                                                 this,
                                                 &RTWorkerCU::setDoubleValue);
    
    addInputDoubleAttributeToDataSet<RTWorkerCU>("bias",
                                                 "The bias of the wave",
                                                 this,
                                                 &RTWorkerCU::setDoubleValue);
    
    
    addInputDoubleAttributeToDataSet<RTWorkerCU>("gain",
                                                 "The gain of the wave",
                                                 this,
                                                 &RTWorkerCU::setDoubleValue);
    
    addInputDoubleAttributeToDataSet<RTWorkerCU>("phase",
                                                 "The phase of the wave",
                                                 this,
                                                 &RTWorkerCU::setDoubleValue);
    
    addInputDoubleAttributeToDataSet<RTWorkerCU>("gain_noise",
                                                 "The gain of the noise of the wave",
                                                 this,
                                                 &RTWorkerCU::setDoubleValue);
}

void RTWorkerCU::unitDefineDriver(std::vector<chaos::cu::driver_manager::driver::DrvRequestInfo>& neededDriver) {
	
}

/*
 Initialize the Custom Contro Unit and return the configuration
 */
void RTWorkerCU::unitInit() throw(CException) {
    LAPP_ << "init RTWorkerCU";
    //RTAbstractControlUnit::init();
    
    initTime = steady_clock::now();
    lastExecutionTime = steady_clock::now();
    numberOfResponse = 0;
    srand((unsigned)time(0));
    PI = acos((long double) -1);
    messageID = 0;
    sinevalue = NULL;
    setWavePoint("points", 30);
    freq = 1.0;
    gain = 5.0;
    phase = 0.0;
    bias = 0.0;
    gainNoise = 0.5;
}

/*
 Execute the work, this is called with a determinated delay, it must be as fast as possible
 */
void RTWorkerCU::unitStart() throw(CException) {
   // RTAbstractControlUnit::start();
}

/*
 Execute the Control Unit work
 */
void RTWorkerCU::unitRun() throw(CException) {
    //get new data wrapper instance filled
    //with mandatory data
    CDataWrapper *acquiredData = getNewDataWrapper();
    if(!acquiredData) return;
    
    //put the messageID for test the lost of package
    acquiredData->addInt32Value("id", ++messageID);
    computeWave(acquiredData);
    
    //submit acquired data
    pushDataSet(acquiredData);
    
}

void RTWorkerCU::computeWave(CDataWrapper *acquiredData) {
    if(sinevalue == NULL) return;
    double interval = (2*PI)/points;
    boost::mutex::scoped_lock lock(pointChangeMutex);
    for(int i=0; i<points; i++){
        sinevalue[i] = (gain*sin((interval*i)+phase)+(((double)randInt()/(double)100)*gainNoise)+bias);
    }
    acquiredData->addBinaryValue("sinWave", (char*)sinevalue, (int32_t)sizeof(double)*points);
}


/*
 Execute the Control Unit work
 */
void RTWorkerCU::unitStop() throw(CException) {
    LAPP_ << "stop RTWorkerCU";
    //RTAbstractControlUnit::stop();
}

/*
 Deinit the Control Unit
 */
void RTWorkerCU::unitDeinit() throw(CException) {
    LAPP_ << "deinit RTWorkerCU";
    if(sinevalue){
        free(sinevalue);
    }
    //RTAbstractControlUnit::deinit();
}

/*
 */
void RTWorkerCU::setWavePoint(const std::string& deviceID, const int32_t& newNumberOfPoints) {
    boost::mutex::scoped_lock lock(pointChangeMutex);
    int32_t tmpNOP = newNumberOfPoints;
    if(tmpNOP < 1) tmpNOP = 0;
    
    if(!tmpNOP){
        if(sinevalue){
            free(sinevalue);
            sinevalue = NULL;
        }
    }else{
        size_t byteSize = sizeof(double) * tmpNOP;
        double* tmpPtr = (double*)realloc(sinevalue, byteSize);
        if(tmpPtr) {
            sinevalue = tmpPtr;
            memset(sinevalue, 0, byteSize);
        }else{
            //memory can't be enlarged so pointer ramin the same
            //so all remain unchanged
            tmpNOP = points;
        }
    }
    points = tmpNOP;
}

/*
 */
void RTWorkerCU::setDoubleValue(const std::string& deviceID, const double& dValue) {
    LAPP_ <<  "setDoubleValue for " << deviceID << " = " << dValue;
    if(!deviceID.compare("frequency")){
        freq = dValue;
    } else if(!deviceID.compare("gain")){
        gain = dValue;
    } else if(!deviceID.compare("phase")){
        phase = dValue;
    } else if(!deviceID.compare("bias")){
        bias = dValue;
    } else if(!deviceID.compare("gain_noise")){
        gainNoise = dValue;
    }
}

/*
 Test Action Handler
 */
CDataWrapper* RTWorkerCU::actionTestOne(CDataWrapper *actionParam, bool& detachParam) {
    CDataWrapper *result = new CDataWrapper();
    result->addStringValue("result_key", "result_key_value");
    return result;
}

/*
 Test Action Handler
 */
CDataWrapper* RTWorkerCU::resetStatistic(CDataWrapper *actionParam, bool& detachParam) {
    LAPP_ << "resetStatistic in RTWorkerCU called from rpc";
    numberOfResponse = 0;
    return NULL;
}

/*
 Test Action Handler
 */
CDataWrapper* RTWorkerCU::actionTestTwo(CDataWrapper *actionParam, bool& detachParam) {
    LAPP_ << "resetStatistic in RTWorkerCU called from rpc";
    if(actionParam->hasKey(ACTION_TWO_PARAM_NAME)){
        int32_t sleepTime =  actionParam->getInt32Value(ACTION_TWO_PARAM_NAME);
        
        LAPP_ << "param for actionTestTwo with value:" << sleepTime;
        LAPP_ << "let this thread to waith "<< sleepTime << " usec";
        boost::this_thread::sleep(boost::posix_time::microseconds(sleepTime));
    } else {
        LAPP_ << "No param received for action actionTestTwo";
    }
    return NULL;
}
