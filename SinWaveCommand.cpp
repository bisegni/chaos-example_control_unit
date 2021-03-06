/*
 *	SinWaveCommand.h
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
#include "SinWaveCommand.h"
#include <boost/lexical_cast.hpp>

#define CMDCU_ LAPP_ << "[SinWaveCommand] - "


using namespace chaos;

using namespace chaos::common::data;

using namespace chaos::cu::control_manager::slow_command;

SinWaveCommand::SinWaveCommand():rng((const uint_fast32_t) time(0) ),one_to_hundred( -100, 100 ),randInt(rng, one_to_hundred) {
    //set default scheduler delay
    setFeatures(features::FeaturesFlagTypes::FF_SET_SCHEDULER_DELAY, 50);
}

SinWaveCommand::~SinWaveCommand() {
    
}

// return the implemented handler
uint8_t SinWaveCommand::implementedHandler() {
    return  HandlerType::HT_Set |
    HandlerType::HT_Acquisition |
	HandlerType::HT_Correlation;
}

// Start the command execution
void SinWaveCommand::setHandler(CDataWrapper *data) {
    // chaos::cu::DeviceSchemaDB *deviceDB = NULL;
    
    srand((unsigned)time(0));
    PI = acos((long double) -1);
    sinevalue = NULL;
    
    
    pointSetting = getValueSetting((AttributeIndexType)0);
    points = pointSetting->getCurrentValue<uint32_t>();
    //setWavePoint();
    *(freq = getValueSetting((AttributeIndexType)1)->getCurrentValue<double>()) = 1.0;
    *(bias = getValueSetting((AttributeIndexType)2)->getCurrentValue<double>()) = 0.0;
    *(phase = getValueSetting((AttributeIndexType)3)->getCurrentValue<double>()) = 0.0;
    *(gain = getValueSetting((AttributeIndexType)4)->getCurrentValue<double>()) = 5.0;
    *(gainNoise = getValueSetting((AttributeIndexType)5)->getCurrentValue<double>()) = 0.5;
    
    //custom variable
    quitSharedVariable = getValueSetting((AttributeIndexType)6)->getCurrentValue<bool>();
    
    lastStartTime = 0;
	
	//this is necessary becase if the command is installed after the first start the
	//attribute is not anymore marched has "changed". so in every case at set handler
	//we call the set point funciton that respect the actual value of poits pointer.
	setWavePoint();
	
    CMDCU_ << "SinWaveCommand::setHandler";
}

// Aquire the necessary data for the command
/*!
 The acquire handler has the purpose to get all necessary data need the by CC handler.
 \return the mask for the runnign state
 */
void SinWaveCommand::acquireHandler() {
    uint64_t timeDiff = shared_stat->lastCmdStepStart - lastStartTime;
    
    if(timeDiff > 10000 || (*quitSharedVariable)) {
        //every ten seconds ste the state until reac the killable and
        //the return to exec
        lastStartTime = shared_stat->lastCmdStepStart;
        if(!(*quitSharedVariable)) {
            switch (SlowCommand::runningState) {
                case chaos::cu::control_manager::slow_command::RunningStateType::RS_Exsc:
                    SL_STACK_RUNNIG_STATE
                    CMDCU_ << "Change to SL_STACK_RUNNIG_STATE";
                    break;
                case chaos::cu::control_manager::slow_command::RunningStateType::RS_Stack:
                    SL_KILL_RUNNIG_STATE
                    CMDCU_ << "Change to SL_KILL_RUNNIG_STATE";
                    break;
                case chaos::cu::control_manager::slow_command::RunningStateType::RS_Kill:
                    SL_EXEC_RUNNIG_STATE
                    CMDCU_ << "Change to SL_EXEC_RUNNIG_STATE";
                    break;
            }
        } else {
            SL_END_RUNNIG_STATE;
        }
    }
    
    //check if some parameter has changed every 100 msec
    if(timeDiff > 100) {
        getChangedAttributeIndex(changedIndex);
        if(changedIndex.size()) {
            CMDCU_ << "We have " << changedIndex.size() << " changed attribute";
            for (int idx =0; idx < changedIndex.size(); idx++) {
                ValueSetting *vSet = getValueSetting(changedIndex[idx]);
				
				//set change as completed
				vSet->completed();
				
                //the index is correlated to the creation sequence so
                //the index 0 is the first input parameter "frequency"
                switch (vSet->index) {
                    case 0://points

                        // apply the modification
                        setWavePoint();
                        break;
                    default:// all other parameter are managed into the function that create the sine waveform
                        break;
                }

            }
            changedIndex.clear();
        }
    }
    CDataWrapper *acquiredData = getNewDataWrapper();
    if(!acquiredData) return;
    
    //put the messageID for test the lost of package
    acquiredData->addInt32Value("id", ++messageID);
    computeWave(acquiredData);
    
    //submit acquired data
    pushDataSet(acquiredData);
}

// Correlation and commit phase
void SinWaveCommand::ccHandler() {
    
}

void SinWaveCommand::computeWave(CDataWrapper *acquiredData) {
    if(sinevalue == NULL) return;
    double interval = (2*PI)/(*points);
    boost::mutex::scoped_lock lock(pointChangeMutex);
    for(int i=0; i<(*points); i++){
        sinevalue[i] = ((*gain)*sin((interval*i)+(*phase))+(((double)randInt()/(double)100)*(*gainNoise))+(*bias));
    }
    acquiredData->addBinaryValue("sinWave", (char*)sinevalue, (int32_t)sizeof(double)*(*points));
}

/*
 */
void SinWaveCommand::setWavePoint() {
    boost::mutex::scoped_lock lock(pointChangeMutex);
    uint32_t tmpNOP = *pointSetting->getCurrentValue<uint32_t>();
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
            pointSetting->completedWithError();
        }
    }
}