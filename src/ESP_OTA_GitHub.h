/*
  ESP_OTA_GitHub.h - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/
#pragma once
#ifndef ESP_OTA_GitHub_h
#define ESP_OTA_GitHub_h
//#define ESPOTAGitHub_DEBUG 

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
//#include "GitHubCertRoot.h"
//#include <time.h>
#include <GSON.h>

#define GHOTA_HOST "api.github.com"
#define GHOTA_PORT 443
#define GHOTA_TIMEOUT 500
#define GHOTA_CONTENT_TYPE "application/octet-stream"

#if not defined GHOTA_REBOOT_AFTER_UPGRADE
#define GHOTA_REBOOT_AFTER_UPGRADE true
#endif

#define GHOTA_NTP1 "ntp1.stratum2.ru"
#define GHOTA_NTP2 "ru.pool.ntp.org"
#define NULL_STR (char*)nullptr


#if defined ESPOTAGitHub_DEBUG
    #warning "ESPOTAGitHub_DEBUG on"
   #define OTAdebugPrint(x)              Serial.print(x)
   #define OTAdebugPrintln(x)            Serial.println(x)
   #define OTAdebugPrintf(s,...)          { Serial.printf((s), __VA_ARGS__); }
   #define OTAdebugPretty              Serial.print(__LINE__); Serial.print(" "); Serial.println(__PRETTY_FUNCTION__)
   #define OTArunStart                 { Serial.print(__PRETTY_FUNCTION__); Serial.println(F(": Start loop ms")); _startMs=millis(); }
   #define OTAprintRunTime                  { auto stop=millis()-_startMs; Serial.print(F("Loop timeMs=")); Serial.println(stop); }
	static unsigned long _startMs;
#else
   #define OTAdebugBegin(x)
   #define OTAdebugPrint(x)
   #define OTAdebugPrintln(x)
   #define OTAdebugPrintf(s,...)
   #define OTAdebugPretty
   #define OTArunStart
   #define OTAprintRunTime
#endif

struct urlDetails_t : public Printable {
    String proto;
    String host;
	int port;
    String path;
	bool zero(){
		proto = NULL_STR;
		host = NULL_STR;
		path = NULL_STR;
		return proto.reserve(0) + host.reserve(0) + path.reserve(0);
	};
	size_t printTo(Print& p) const {
		size_t out =0;
		out += p.print("Proto:");
		out += p.println(proto);

		out += p.print("Host:");
		out += p.println(host);

		out += p.print("Port:");
		out += p.println(port);

		out += p.print("Path:\"");
		out += p.print(path);
		out += p.println('"');
		return out;
	};
};

// namespace myHTTP_Request {
// 	void myHTTPGet(WiFiClientSecure& client, const char* host, ... ){
// 		    va_list args;
//     		va_start(args, host);

// 		client.print(F("GET "));
// 		while(true) {
// 			char * path = va_arg(args, char*);
// 			if ( path == nullptr) break;
// 			if ( path[0] != '/' ) client.print('/');
// 			client.print(path);
// 		}
// 			va_end(args);
// 		client.print(F(" HTTP/1.1\r\n"));
// 		client.print(F("Host: ") ); client.print( host );
// 		client.print(F("\r\n"));
// 		client.print(F("User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"));
// 		client.print(F("Connection: close\r\n\r\n"));
// 	};
// };

class ESPOTAGitHub {
	public:
		ESPOTAGitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease);
		ESPOTAGitHub(const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease);
		ESPOTAGitHub(BearSSL::X509List* cert, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease);
		bool checkUpgrade();
		bool doUpgrade();
		bool doUpgrade(String&);
		bool clean();
		String getLastError();
		String getUpgradeURL();
		String getLatestTag();
		String releaseNote;
		//#if defined CLEANING
		
		//#endif
	static void _HTTPget(WiFiClientSecure& client, const char* host, ... ) {
		va_list args;
		va_start(args, host);

		client.print(F("GET "));
		OTAdebugPrint(F("GET "));

		while(true) {
			auto path = va_arg(args, const char*);
			if ( path == nullptr) break;
			if ( path[0] != '/' ) {
				client.print('/');
				OTAdebugPrint('/');
			}
			client.print(path);
			OTAdebugPrint(path);
		}
		OTAdebugPrint(F(" to host:")); OTAdebugPrintln( host ); 
		va_end(args);
		client.println(F(" HTTP/1.1"));
		client.print(F("Host: ") ); client.println( host );
		client.println(F("User-Agent: ESP8266\r\n" \
						"Connection: close\r\n"));
	};
	private:
		
		void _init(const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) {
			_user = user;
			_repo = repo;
			_currentTag = currentTag;
			_binFile = binFile;
			_preRelease = preRelease;
			_lastError = NULL_STR;
			_upgradeURL = NULL_STR;
			_releaseTag = NULL_STR;
		};
		static const long long _likeRealTime = 8 *3600 *2;
		bool _useInsecureConnection(){
			return ( _certStore == nullptr && _cert == nullptr ); 
		};
		void _setClock(); //Set time via NTP, as required for x.509 validation
		urlDetails_t _urlDetails(String url, urlDetails_t * splitDetailP); // Separates a URL into protocol, host and path into a custom struct
		//urlDetails_t _urlDetails(urlDetails_t& splitDetail, String url); // Separates a URL into protocol, host and path into a custom struct

		bool _resolveRedirects(); // Follows re-direct sequences until a "real" url is found.
		bool _setClientSecure(BearSSL::WiFiClientSecure& ); 
		
		 BearSSL::CertStore* _certStore = nullptr;
		 BearSSL::X509List* _cert = nullptr;
		String _lastError; // Holds the last error generated
		String _upgradeURL; // Holds the upgrade URL (changes when getFinalURL() is run).
		String _releaseTag;
		const char* _user;
		const char* _repo;
		const char* _currentTag;
		const char* _binFile;
		bool _preRelease;
};

#endif
