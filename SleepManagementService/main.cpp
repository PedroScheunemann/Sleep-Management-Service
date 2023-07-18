#include "globals.cpp"

// Funções do participante
static void guest_program();
void* guest_discovery_service(void *arg);
void* guest_monitoring_service(void *arg);
void* guest_interface_service(void *arg);
string get_host_name();
string get_mac_address();
string get_ip_address();
string get_discover_message();

// Funções do manager
static void manager_program();
void* manager_discovery_service(void *arg);
void* manager_monitoring_service(void *arg);
void* manager_interface_service(void *arg);
guest parse_payload(string payload);

// cout << "oi" << endl;
// cout << "hello" << endl;

int main(int argc, char **argv)
{
    cout << "./sleep_server OR ./sleep_server manager" << endl;

    // ./sleep_server
    if (argc == 1)
    {
        guest_program();
    }
    // ./sleep_server manager
    else if (argc == 2 && strcmp(argv[1], "manager") == 0)
    {
        manager_program();
    }
}

static void guest_program()
{
    pthread_t thread_discovery, thread_monitoring, thread_interface;
    pthread_attr_t attributes_thread_discovery, attributes_thread_monitoring, attributes_thread_interface;

    // inicializando os atributos das threads
    if (pthread_attr_init(&attributes_thread_discovery) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação dos atributos da threads de monitoramento." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_interface)!= 0)
    {
        cout << "PARTICIPANTE: Erro na criação dos atributos da thread da interface." << endl;
        exit(1);
    }

    // criando as threads
    if (pthread_create(&thread_discovery, &attributes_thread_discovery, guest_discovery_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, guest_monitoring_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da threads de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, &attributes_thread_interface, guest_interface_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread da interface." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    pthread_join(thread_discovery, nullptr);
    pthread_join(thread_monitoring, nullptr);
    pthread_join(thread_interface, nullptr);

    // destruindo atributos
    pthread_attr_destroy(&attributes_thread_discovery);
    pthread_attr_destroy(&attributes_thread_monitoring);
    pthread_attr_destroy(&attributes_thread_interface);
}

static void manager_program()
{
    pthread_t thread_discovery, thread_monitoring, thread_interface;
    pthread_attr_t attributes_thread_discovery, attributes_thread_monitoring, attributes_thread_interface;

    // inicializando os atributos das threads
    if (pthread_attr_init(&attributes_thread_discovery) != 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_interface)!= 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread da interface." << endl;
        exit(1);
    }

    // criando as threads
    if (pthread_create(&thread_discovery, &attributes_thread_discovery, manager_discovery_service , nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, manager_monitoring_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, &attributes_thread_interface, manager_interface_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread da interface." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    pthread_join(thread_discovery, nullptr);
    pthread_join(thread_monitoring, nullptr);
    pthread_join(thread_interface, nullptr);

    // destruindo atributos
    pthread_attr_destroy(&attributes_thread_discovery);
    pthread_attr_destroy(&attributes_thread_monitoring);
    pthread_attr_destroy(&attributes_thread_interface);
}

// Envia pacotes do tipo SLEEP SERVICE DISCOVERY para as estações por broadcast e espera ser respondido.
// Cliente -> Envia -> Porta do manager;
void* guest_discovery_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Setando o socket para a opção broadcast
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo broadcast do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de descoberta
    struct sockaddr_in discovery_server_address;
    discovery_server_address.sin_family = AF_INET;
	discovery_server_address.sin_port = htons(PORT_DISCOVERY_SERVICE);
	discovery_server_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    socklen_t discovery_server_address_len = sizeof(struct sockaddr_in);

    // Faz uma mensagem com informações de hostname;mac_address;ip_address para ser enviada
    size_t  message_len = sizeof(packet);
    string message = get_discover_message();
    packet* discover_message = new packet();
    discover_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(discover_message->payload, message);

    // Inicializando o espaço de memória que vai receber a mensagem de resposta do manager.
    packet* manager_message = new packet();

    // Inicializando o espaço de memória que vai receber o endereço do manager.
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);


    // Enviando mensagem para todo mundo
    if (sendto(socket_descriptor, discover_message, message_len, 0,(struct sockaddr *) &discovery_server_address, &discovery_server_address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao enviar mensagem no servidor de descoberta." << endl;
    }
    // Esperando resposta chegar
    if (recvfrom(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager pelo servidor de descoberta." << endl;
    }
    else
    {
        // Até o manager responder
        if (manager_message->type == SLEEP_SERVICE_DISCOVERY)
        {
            // Manager mandou id de identificação do participante
            current_guest_id = atoi(manager_message);
            break;
        }
    }

    free(discover_message);
    free(manager_message);

    close(socket_descriptor);
    return nullptr;
}

// Recebendo e respondendo pacotes do tipo SLEEP STATUS REQUEST para o manager.
// Servidor -> Escuta -> Uma porta para o manager // Manager usa uma porta diferente para cada participante, portanto a porta vai depender do ID.
void* guest_monitoring_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Setando o socket para a opção REUSEADDR
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo REUSEADDR do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de monitoramento
    struct sockaddr_in monitoring_server_address;
    monitoring_server_address.sin_family = AF_INET;
	monitoring_server_address.sin_port = htons(PORT_MONITORING_SERVICE + current_guest_id);
	monitoring_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t monitoring_server_address_len = sizeof(struct sockaddr_in);

    // Endereçando o socket ao servidor de monitoramento
    if (bind(socket_descriptor, (struct sockaddr*) &monitoring_server_address, monitoring_server_address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endereçar o socket do servidor de monitoramento." << endl;
        exit(1);
    }

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do manager.
    packet* manager_message = new packet();
    // Inicializando o espaço de memória da mensagem em resposta ao manager.
    packet* awake_message = new packet();
    awake_message->type = SLEEP_STATUS_REQUEST;

    // Inicializando o espaço de memória que vai receber o endereço do manager.
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);


    while (1)
    {
        // Espera receber mensagem do manager
        if (recvfrom(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao receber mensagem do manager no servidor de monitoramento." << endl;
        }
        else
        {
            // Se manager mandou mensagem de monitoramento, responde
            if (manager_message->type == SLEEP_STATUS_REQUEST)
            {
                // Se o participante quer sair do serviço, envia mensagem avisando
                if (current_guest_is_out == 1)
                {
                    strcpy(awake_message->payload, STATUS_QUIT);
                }
                // Senão, envia mensagem avisando que está acordado
                else
                {
                    strcpy(awake_message->payload, STATUS_AWAKE);
                }
                if (sendto(socket_descriptor, awake_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
                {
                    cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager pelo servidor de monitoramento." << endl;
                }
            }
        }
    }
    free(manager_message);
    free(awake_message);

    close(socket_descriptor);
    return nullptr;
}

// TO DO: Fica processando a entrada do usuário pra ver se vai vir um pedido de EXIT
void* guest_interface_service(void *arg)
{

    // current_guest_is_out = 1;
}

// Recebendo e respondendo pacotes do tipo SLEEP SERVICE DISCOVERY.
// Servidor -> Escuta -> Uma porta para todos os participantes;
void* manager_discovery_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Setando o socket para a opção broadcast
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo broadcast do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de descoberta
    struct sockaddr_in discovery_server_address;
    discovery_server_address.sin_family = AF_INET;
	discovery_server_address.sin_port = htons(PORT_DISCOVERY_SERVICE);
	discovery_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t discovery_server_address_len = sizeof(struct sockaddr_in);

    // Endereçando o socket ao servidor de descoberta
    if (bind(socket_descriptor, (struct sockaddr*) &discovery_server_address, discovery_server_address_len) < 0)
    {
        cout << "MANAGER: Erro ao endereçar o socket do servidor de descoberta." << endl;
        exit(1);
    }

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do participante
    packet* guest_message = new packet();
    // Inicializando o espaço de memória da mensagem em resposta ao participante
    packet* discovered_message = new packet();
    discovered_message->type = SLEEP_SERVICE_DISCOVERY;

    // Inicializando o espaço de memória que vai receber o endereço do participante
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);

    while (1)
    {
        // Espera receber mensagem de algum participante
        if (recvfrom(socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
        {
            cout << "MANAGER: Erro ao receber mensagem de participante no servidor de descoberta." << endl;
        }
        else
        {
            // Se encontrou um participante, envia resposta
            if (guest_message->type == SLEEP_SERVICE_DISCOVERY)
            {
                // A mensagem será o identificador do participante
                strcpy(discovered_message->payload, to_string(guests_id));
                guests_id++;

                if (sendto(socket_descriptor, discovered_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
                {
                    cout << "MANAGER: Erro ao enviar mensagem a participante pelo servidor de descoberta." << endl;
                }
                // Adiciona na tabela de paticipantes
                guest new_guest = parse_payload(guest_message->payload);
                new_guest.status = "awake";

                // Garantindo acesso exclusivo a tabela
                pthread_mutex_lock(&mutex_table);
                guests.addGuest(new_guest);
                pthread_mutex_unlock(&mutex_table);
            }
        }
    }
    free(guest_message);
    free(discovered_message);

    close(socket_descriptor);
    return nullptr;
}

// Enviando pacotes do tipo SLEEP STATUS REQUEST para as estações já conhecidas e processa resposta.
// Cliente -> Envia -> Uma porta para cada participante;
void* manager_monitoring_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Setando o socket para a opção REUSEADDR
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo REUSEADDR do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "MANAGER: Erro ao setar o timeout do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Configurando as infos iniciais do endereço do servidor do participante
    struct sockaddr_in guest_server_address;
    guest_server_address.sin_family = AF_INET;
    socklen_t guest_server_address_len = sizeof(struct sockaddr_in);

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do participante
    packet* guest_message = new packet();
    // Inicializando o espaço de memória da mensagem do manager
    packet* manager_message = new packet();
    manager_message->type = SLEEP_STATUS_REQUEST;
    strcpy(manager_message->payload, STATUS_REQUEST);

    while (1)
    {
        int guest_id = 0;

        pthread_mutex_lock(&mutex_table);
        list<string> guests_list_ip_adresses = guests.returnGuestsIpAdress();
        pthread_mutex_unlock(&mutex_table);

        // Envia mensagens para todos os participantes
        for(list<string>::iterator it = guests_list_ip_adresses.begin(); it != guests_list_ip_adresses.end(); ++it)
        {
            guest_server_address.sin_port = htons(PORT_MONITORING_SERVICE+guest_id); // Porta diferente para cada participante
            inet_aton(it->c_str(), (in_addr *) &guest_server_address.sin_addr.s_addr);

            if (sendto(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_server_address, &guest_server_address_len) < 0) {
                cout << "MANAGER: Erro ao enviar mensagem para algum participante." << endl;
            }

            // Esperando resposta chegar
            if (recvfrom(socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_server_address, &guest_server_address_len) < 0)
            {
                // Se resposta não chegou, depois do timeout, atualiza status do participante para "asleep"
                pthread_mutex_lock(&mutex_table);
                guests.guestSlept(*it);
                pthread_mutex_unlock(&mutex_table);
            }
            else
            {
                // Se resposta chegou, e foi do tipo SLEEP_STATUS_REQUEST
                if (guest_message->type == SLEEP_STATUS_REQUEST)
                {
                    // Se foi avisando que está acordado, atualiza status do participante para "awake"
                    if (strcmp(guest_message->payload, STATUS_AWAKE) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        guests.wakeGuest(*it);
                        pthread_mutex_unlock(&mutex_table);
                    }
                    // Se foi avisando que está saindo, remove participante da tabela
                    if (strcmp(guest_message->payload, STATUS_QUIT) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        guests.deleteGuest(*it);
                        pthread_mutex_unlock(&mutex_table);
                    }
                }
            }
            guest_id++;
        }
    }
    free(guest_message);
    free(discovered_message);

    close(socket_descriptor);
    return nullptr;
}

// TO DO: Fica processando a entrada do usuário pra ver se vai vir um pedido de WAKEUP hostname -> fazer via wake on lan
void* manager_interface_service(void *arg)
{

    return nullptr;
}

// TO DO: pega o nome do host
string get_host_name()
{

}

// TO DO: pega o endereço mac do host
string get_mac_address()
{

}

// TO DO: pega o endereço IP do host
string get_ip_address()
{


}

string get_discover_message()
{
    // Coleta nome do host, endereço mac e endereço IP
    string hostname = get_host_name();
    string mac_address = get_mac_address();
    string ip_address = get_ip_address();

    string message = hostname + ";" + mac_address + ";" + ip_address + ";";
    return message;
}

guest parse_payload(string payload)
{
    guest guest_payload;
    string delimiter = ";";
    string temp_payload = payload;

    string hostname = temp_payload.substr(0, temp_payload.find(delimiter));
    temp_payload.erase(0, temp_payload.find(delimiter) + delimiter.length());

    string mac_address = temp_payload.substr(0, temp_payload.find(delimiter));
    temp_payload.erase(0, temp_payload.find(delimiter) + delimiter.length());

    string ip_address = temp_payload.substr(0, temp_payload.find(delimiter));

    guest_payload.hostname = hostname;
    guest_payload.mac_address = mac_address;
    guest_payload.ip_address = ip_address;

    return guest_payload;
}


