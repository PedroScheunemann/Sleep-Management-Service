using namespace std;

#include <iostream>
#include <cstring>
#include <pthread.h>
#include <string>
#include <list>
#include <map>
#include <iomanip>
//#include <csignal>
//#include <ifaddrs.h>
//#include <cstdio>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <unistd.h>
//#include <cstdlib>
//#include <mutex>

#define PORT_DISCOVERY_SERVICE   8000
#define PORT_MONITORING_SERVICE  8001

#define SLEEP_SERVICE_DISCOVERY  1
#define SLEEP_STATUS_REQUEST     2

#define STATUS_REQUEST           "sleep status needed"
#define STATUS_AWAKE             "sleep status awake"
#define GUEST_DISCOVERED         "discovered"

struct packet {
    uint16_t type;
    char* payload[256];
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
            g.status = "awake";
            guestList.insert_or_assign(g.ip_address, g);
        }

        void deleteGuest(string ip_address)
        {
            guestList.erase(ip_address);
        }

        void putGuestToSleep(string ip_address)
        {
            guestList.at(ip_address).status = "asleep";
        }

        void wakeGuest(string ip_address)
        {
            guestList.at(ip_address).status = "awake";
        }

        //string getGuestMacAdress(string ip_address);

        //string getParticipantStatus(string ip_address);

        void printTable() {
            //system("clear");
            cout << std::left;
            cout << "--------------------------------------------------------------------------\n";
            cout << "|Hostname \t|MAC Address      |IP Address     |Status|\n";
            for (auto &ent: guestList) {
                cout << "|" << setw(15) << ent.second.hostname;
                cout << "|" << ent.second.mac_address;
                cout << "|" << setw(15) << ent.second.ip_address;
                cout << "|" << setw(6) << ent.second.status << "|\n";
            }
            cout << "--------------------------------------------------------------------------\n";
        }
};




