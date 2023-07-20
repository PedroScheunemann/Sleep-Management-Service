using namespace std;

#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <list>
#include <map>
#include <iomanip>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <csignal>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>
#include <cstdlib>
#include <ifaddrs.h>

#define PORT_DISCOVERY_SERVICE   8000
#define PORT_MONITORING_SERVICE  4000

#define SLEEP_SERVICE_DISCOVERY  1
#define SLEEP_STATUS_REQUEST     2

#define GUEST_DISCOVERED         "I FOUND YOU"
#define STATUS_REQUEST           "sleep status needed"
#define STATUS_AWAKE             "sleep status awake"
#define STATUS_QUIT              "sleep status quit"

struct packet {
    uint16_t type;
    char payload[256];
};

struct guest{
    string hostname;
    string mac_address;
    string ip_address;
    string status;
};

class guestTable{

    public:
        guestTable(){}

        map<string, guest> guestList;

        void addGuest(guest g)
        {
            guestList.insert_or_assign(g.ip_address, g);
        }

        void deleteGuest(string ip_address)
        {
            guestList.erase(ip_address);
        }

        void guestSlept(string ip_address)
        {
            guestList.at(ip_address).status = "asleep";
        }

        void wakeGuest(string ip_address)
        {
            guestList.at(ip_address).status = "awake";
        }

        string getGuestStatus(string ip_address)
        {
            return guestList.at(ip_address).status;
        }

        vector<string> returnGuestsIpAddressList()
        {
            vector<string> list_ip_addresses;
            for (auto &i: guestList) {
                list_ip_addresses.push_back(i.second.ip_address);
            }
            return list_ip_addresses;
        }

        string returnGuestMacAddress(string hostname)
        {
            for (auto &i: guestList) {
                if (i.second.hostname == hostname)
                {
                    return i.second.mac_address;
                }
            }
            return "";
        }

        void printTable() {
            system("clear");
            cout << left;
            cout << "--------------------------------------------------------------------------\n";
            cout << "|Hostname \t|MAC Address      |IP Address     |Status|\n";
            for(auto &i: guestList)
            {
                cout << "|" << setw(15) << i.second.hostname;
                cout << "|" << i.second.mac_address;
                cout << "|" << setw(15) << i.second.ip_address;
                cout << "|" << setw(6) << i.second.status << "|\n";
            }
            cout << "--------------------------------------------------------------------------\n";
        }
};




