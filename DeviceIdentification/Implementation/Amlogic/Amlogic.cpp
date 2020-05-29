/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../../Module.h"
#include <interfaces/IDeviceIdentification.h>

#include <fstream>

extern "C" {
#include <mfrApi.h>
}

namespace WPEFramework {
namespace Plugin {

class DeviceImplementation : public Exchange::IDeviceProperties, public PluginHost::ISubSystem::IIdentifier {

public:
    DeviceImplementation()
    {
        UpdateChipset(_chipset);
        UpdateFirmwareVersion(_firmwareVersion);
    }

    DeviceImplementation(const DeviceImplementation&) = delete;
    DeviceImplementation& operator= (const DeviceImplementation&) = delete;
    virtual ~DeviceImplementation()
    {
    	/* Nothing to do here. */
    }

public:
    // Device Propertirs interface
    const string Chipset() const override
    {
        return _chipset;
    }
    const string FirmwareVersion() const override
    {
        return _firmwareVersion;
    }

    // Identifier interface
    uint8_t Identifier(const uint8_t length, uint8_t buffer[]) const override
    {
        return 0;
    }

    BEGIN_INTERFACE_MAP(DeviceImplementation)
        INTERFACE_ENTRY(Exchange::IDeviceProperties)
        INTERFACE_ENTRY(PluginHost::ISubSystem::IIdentifier)
    END_INTERFACE_MAP

private:
    inline void UpdateFirmwareVersion(string& firmwareVersion) const
    {
       int retVal = -1;
	mfrSerializedData_t mfrSerializedData;
	retVal = mfrGetSerializedData(mfrSERIALIZED_TYPE_SOFTWAREVERSION, &mfrSerializedData);
	if ((mfrERR_NONE == retVal) && mfrSerializedData.bufLen) {
		firmwareVersion =  mfrSerializedData.buf;
		if (mfrSerializedData.freeBuf) {
			mfrSerializedData.freeBuf(mfrSerializedData.buf);
		}
	}
    }
    inline void UpdateChipset(string& chipset) const
    {
       int retVal = -1;
	mfrSerializedData_t mfrSerializedData;
	retVal = mfrGetSerializedData(mfrSERIALIZED_TYPE_CHIPSETINFO, &mfrSerializedData);
	if ((mfrERR_NONE == retVal) && mfrSerializedData.bufLen) {
		chipset = mfrSerializedData.buf;
		if (mfrSerializedData.freeBuf) {
			mfrSerializedData.freeBuf(mfrSerializedData.buf);
		}
	}
    }

private:
    string _chipset;
    string _firmwareVersion;
};

    SERVICE_REGISTRATION(DeviceImplementation, 1, 0);
}
}
