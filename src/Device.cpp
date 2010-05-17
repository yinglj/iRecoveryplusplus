/***
 * iRecovery++ libusb based usb interface for iBoot and iBSS
 * Copyright � 2010  GreySyntax
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "headers/Device.h"

using namespace std;

Device::Device() {

	libusb_init(NULL);
}

bool Device::AutoBoot() {

	if(SendCommand("setenv auto-boot true")) {
		if (SendCommand("saveenv")) {
			if (SendCommand("reboot")) {

				cout << "[Info] Enabled auto-boot." << endl;
				return true;
			}
		}
	}

	cout << "[Info] Failed to enable auto-boot." << endl;
	return false;
}

bool Device::Connect() {

	if ((device = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, RECV_MODE)) == NULL) {
		if ((device = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, WTF_MODE)) == NULL) {
			if ((device = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, DFU_MODE)) == NULL) {
				return false;
			}
		}
	}

	int configuration = 0;
	libusb_get_configuration(device, &configuration);

	if (configuration != 1) {

		if(libusb_set_configuration(device, 1) < 0) {

			cout << "[Info] Failed to set configuration." << endl;
			return false;
		}
	}

	if (libusb_claim_interface(device, 0) < 0) {
		cout << "[Info] Error claiming interface." << endl;
		//If at first you don't succeed, try, try again.
		//Then quit. There's no point in being a damn fool about it.
		return false;
	}

	//It's not enough that I should succeed - others should fail.
	return true;
}

void Device::Disconnect() {

	if (device != NULL) {
		cout << "[Info] Closing USB Connection." << endl;
		libusb_release_interface(device, 0);
		libusb_release_interface(device, 1);
		libusb_close(device);
	}
}

bool Device::Exploit(const char* file) {

	cout << "[Info] Attempting to send exploit" << endl;

	if (Upload(file)) {

		if (! libusb_control_transfer(device, 0x21, 2, 0, 0, 0, 0, 1000)) {

			cout << "[Info] Failed to send exploit" << endl;
			return false;
		}

		cout << "[Info] Succesfully sent exploit" << endl;
		return true;
	}

	cout << "[Info] Failed to sent exploit" << endl;
	return false;
}

bool Device::IsConnected() {

	return device == NULL ? false : true;
}

void Device::Reset() {

	libusb_reset_device(device);
}

bool Device::SendBuffer(char* buffer, int index, int length) {

	int packets, last, pos = (length - index);

	packets = pos % 0x800;
	last = pos % 0x800;

	if (pos % 0x800) packets++;
	if (! last) last = 0x800;

	unsigned int sent;
	char response[6];

	for (int i = 0; i < packets; i++) {

		int len = last;

		if ((i + 1) < packets) len = 0x800;
			sent += len;

		if (libusb_control_transfer(device, 0x21, 1, i, 0, (unsigned char*)&buffer[i * 0x800], len, 1000)) {

			if (libusb_control_transfer(device, 0xA1, 3, 0, 0, (unsigned char*)response, 6, 1000) == 6) {

				if (response[4] == 5) {

					cout << "[IO] Sucessfully transfered " << (i + 1) << " of " << packets << endl;
					continue;
				}

				cout << "[Info] Invalid execution status" << endl;
				free(buffer);
				return false;
			}

			cout << "[Info] Failed to retreive execution status" << endl;
			free(buffer);
			return false;
		}

		cout << "[IO] Failed to send packet " << (i + 1) << " of " << packets << endl;
		free(buffer);
		return false;
	}

	libusb_control_transfer(device, 0x21, 1, packets, 0, (unsigned char*)buffer, 0, 1000);
	free(buffer);
	cout << "[Info] Executing" << endl;

	for (int i = 6; i <= 8; i++) {

		if (libusb_control_transfer(device, 0xA1, 3, 0, 0, (unsigned char*)response, 6, 1000) == 6) {
			if (response[4] != i) {

				cout << "[Info] Invalid status recived from device" << endl;
				return false;
			}
			continue;
		}
		return false;
	}

	cout << "[Info] Successfully transfered buffer" << endl;
	return true;
}

bool Device::SendCommand(const char* command) {

	int length = strlen(command);

	if (length >= 0x100) {
		cout << "[Info] Failed to send command (to long)." << endl;
		return false;
	}

	if(! libusb_control_transfer(device, 0x40, 0, 0, 0, (unsigned char*)command, (length + 1), 500)) {
		cout << "[Info] Failed to send command" << endl;
		return false;
	}

	cout << "[Info] Sent " << command << " to device" << endl;
	return true;
}

bool Device::Upload(const char* file) {

	FILE* data = fopen(file, "rb");

	if (data != NULL) {

		cout << "[Info] Attemtping to upload file" << endl;

		fseek(data, 0, SEEK_END);
		unsigned int length = ftell(data);
		fseek(data, 0, SEEK_SET);

		char* buffer = (char*)malloc(length);

		if (buffer != NULL) {

			fread(buffer, 1, length, data);
			fclose(data);

			return SendBuffer(buffer, 0, length);

		}

		cout << "[IO] Failed to allocate " << length << " bytes" << endl;
		fclose(data);
		return false;
	}

	cout << "[IO] Failed to open file " << file << endl;
	return false;
}
