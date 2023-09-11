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
#define PORT_ELECTION_SERVICE    2000

#define SLEEP_SERVICE_DISCOVERY  1
#define GUEST_DISCOVERED         2

#define SLEEP_STATUS_REQUEST     3
#define STATUS_AWAKE             4
#define STATUS_QUIT              5

#define ELECTION                 6
#define COORDINATOR              7
#define ANSWER                   8

struct packet {
    uint16_t type;
    char payload[256];
    guestTable guests;
};

struct guest{
    string hostname;
    string mac_address;
    string ip_address;
    string status;
};

class guestTable{

    public:
        map<string, guest> guestList;
        int version = 0;

        guestTable(){}

        guestTable operator=(guestTable& guests)
        {
            version = guests.version;

            if (!guestList.empty())
            {
                guestList.clear()
            }

            for (auto &it: guests)
            {
                guestList.addGuest(it.second);
            }
        }

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
            for (auto &it: guestList) {
                list_ip_addresses.push_back(it.second.ip_address);
            }
            return list_ip_addresses;
        }

        string returnGuestMacAddress(string hostname)
        {
            for (auto &it: guestList) {
                if (it.second.hostname == hostname)
                {
                    return it.second.mac_address;
                }
            }
            return "";
        }

        void printTable() {
            system("clear");
            cout << left;
            cout << "--------------------------------------------------------------------------\n";
            cout << "|Hostname \t|MAC Address      |IP Address     |Status|\n";
            for(auto &it: guestList)
            {
                cout << "|" << setw(15) << it.second.hostname;
                cout << "|" << it.second.mac_address;
                cout << "|" << setw(15) << it.second.ip_address;
                cout << "|" << setw(6) << it.second.status << "|\n";
            }
            cout << "--------------------------------------------------------------------------\n";
        }
};




