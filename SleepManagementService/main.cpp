#include "globals.cpp"

// Fun��es do participante
static void guest_program();
void* guest_discovery_service(void *arg);
void* guest_monitoring_service(void *arg);
void* guest_interface_service(void *arg);
string get_host_name();
string get_mac_address();
string get_ip_address();
string pack_discovery_sending_message();

// Fun��es do manager
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
        cout << "PARTICIPANTE: Erro na cria��o dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o dos atributos da threads de monitoramento." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_interface)!= 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o dos atributos da thread da interface." << endl;
        exit(1);
    }

    // criando as threads
    if (pthread_create(&thread_discovery, &attributes_thread_discovery, guest_discovery_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, guest_monitoring_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o da threads de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, &attributes_thread_interface, guest_interface_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o da thread da interface." << endl;
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
        cout << "MANAGER: Erro na cria��o dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "MANAGER: Erro na cria��o dos atributos da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_interface)!= 0)
    {
        cout << "MANAGER: Erro na cria��o dos atributos da thread da interface." << endl;
        exit(1);
    }

    // criando as threads
    if (pthread_create(&thread_discovery, &attributes_thread_discovery, manager_discovery_service , nullptr) != 0)
    {
        cout << "MANAGER: Erro na cria��o da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, manager_monitoring_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na cria��o da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, &attributes_thread_interface, manager_interface_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na cria��o da thread da interface." << endl;
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

// Envia pacotes do tipo SLEEP SERVICE DISCOVERY para as esta��es por broadcast e espera ser respondido pelo manager.
// CLIENTE
void* guest_discovery_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    // Setando o socket para a op��o de broadcast (habilitar o envio de broadcast messages)
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo broadcast do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in sending_address;     // endere�o para onde mandaremos mensagem
    struct sockaddr_in manager_address;     // endere�o de onde vai vir a resposta (que ser� do manager)
    socklen_t address_len = sizeof(struct sockaddr_in);

    sending_address.sin_family = AF_INET;
	sending_address.sin_port = htons(PORT_DISCOVERY_SERVICE);
	sending_address.sin_addr.s_addr = htonl(INADDR_BROADCAST); // envia para todos os IPs da rede nas portas PORT_DISCOVERY_SERVICE
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* sending_message = new packet();  // mensagem que ser� enviada para todas as esta��es (hostname;mac_address;ip_address;)
    packet* manager_message = new packet();  // mensagem de resposta que ser� recebida do manager
    size_t  message_len = sizeof(packet);

    sending_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(sending_message->payload, pack_discovery_sending_message());
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Enviando mensagem para todo mundo
    if (sendto(guest_socket_descriptor, sending_message, message_len, 0,(struct sockaddr *) &sending_address, &address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao enviar mensagem de descoberta." << endl;
        exit(1);
    }
    // Esperando resposta do manager chegar
    if (recvfrom(guest_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao receber mensagem do manager de resposta de descoberta." << endl;
        exit(1);
    }

    free(sending_message);
    free(manager_message);

    close(guest_socket_descriptor);
    return nullptr;
}

// Recebendo e respondendo pacotes do tipo SLEEP STATUS REQUEST para o manager.
// SERVIDOR
void* guest_monitoring_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na cria��o do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando a a op��o REUSEADDR do socket (v�rios participantes se conectando na mesma porta do manager)
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo REUSEADDR do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in guest_address;        // endere�o local do guest
    struct sockaddr_in manager_address;      // endere�o de onde vem o pacote (que ser� do manager)
    socklen_t address_len = sizeof(struct sockaddr_in);

    guest_address.sin_family = AF_INET;
	guest_address.sin_port = htons(PORT_MONITORING_SERVICE);  // recebe na porta PORT_MONITORING_SERVICE
	guest_address.sin_addr.s_addr = htonl(INADDR_ANY);        // qualquer IP da m�quina
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Endere�ando o socket
    if (bind(guest_socket_descriptor, (struct sockaddr*) &guest_address, &address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endere�ar o socket do servidor de monitoramento." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* manager_message = new packet();    // mensagem que ser� recebida do manager
    packet* guest_message = new packet();      // mensagem de resposta que ser� enviada pelo guest
    size_t  message_len = sizeof(packet);

    guest_message->type = SLEEP_STATUS_REQUEST;
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Espera receber mensagem do manager
        if (recvfrom(guest_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao receber mensagem do manager [MONITORAMENTO]." << endl;
        }
        else
        {
            // Se manager mandou mensagem de monitoramento, responde
            if (manager_message->type == SLEEP_STATUS_REQUEST)
            {
                // Se o participante quer sair do servi�o, envia mensagem avisando que quer sair
                if (current_guest_is_out)
                {
                    strcpy(guest_message->payload, STATUS_QUIT);
                }
                // Sen�o, envia mensagem avisando que est� acordado
                else
                {
                    strcpy(guest_message->payload, STATUS_AWAKE);
                }
                if (sendto(guest_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
                {
                    cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager [MONITORAMENTO]." << endl;
                }
            }
        }
    }
    free(manager_message);
    free(guest_message);

    close(guest_socket_descriptor);
    return nullptr;
}

// TO DO: Fica processando a entrada do usu�rio pra ver se vai vir um pedido de EXIT
void* guest_interface_service(void *arg)
{

    // current_guest_is_out = 1;
}

// Recebendo e respondendo pacotes do tipo SLEEP SERVICE DISCOVERY que chegaram atrav�s de broadcast.
// SERVIDOR
void* manager_discovery_service(void *arg)
{
    // Criando o descritor do socket do manager
    int manager_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (manager_socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na cria��o do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    // Setando o socket para a op��o broadcast (habilitar o receive de broadcast messages)
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo broadcast do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in manager_address;      // endere�o local do manager
    struct sockaddr_in guest_address;        // endere�o de onde vem o pacote (que ser� do participante)
    socklen_t address_len = sizeof(struct sockaddr_in);

    manager_address.sin_family = AF_INET;
	manager_address.sin_port = htons(PORT_DISCOVERY_SERVICE);  // recebe na porta PORT_DISCOVERY_SERVICE
	manager_address.sin_addr.s_addr = htonl(INADDR_ANY);       // qualquer IP da m�quina
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Endere�ando o socket
    if (bind(manager_socket_descriptor, (struct sockaddr*) &manager_address, &address_len) < 0)
    {
        cout << "MANAGER: Erro ao endere�ar o socket [DESCOBERTA]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* guest_message = new packet();    // mensagem que ser� recebida do participante
    packet* manager_message = new packet();  // mensagem de resposta que ser� enviada pelo manager
    size_t  message_len = sizeof(packet);

    manager_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(manager_message->payload, GUEST_DISCOVERED);
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Espera receber mensagem de algum participante
        if (recvfrom(manager_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
        {
            cout << "MANAGER: Erro ao receber mensagem de participante [DESCOBERTA]." << endl;
        }
        else
        {
            // Se encontrou um participante, envia resposta
            if (guest_message->type == SLEEP_SERVICE_DISCOVERY)
            {
                if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
                {
                    cout << "MANAGER: Erro ao enviar mensagem a participante [DESCOBERTA]." << endl;
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
    free(manager_message);

    close(manager_socket_descriptor);
    return nullptr;
}

// Enviando pacotes do tipo SLEEP STATUS REQUEST para as esta��es j� conhecidas e processa resposta.
// CLIENTE
void* manager_monitoring_service(void *arg)
{
    // Criando o descritor do socket do manager
    int manager_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (manager_socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na cria��o do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando a a op��o REUSEADDR do socket (v�rios participantes se conectando na mesma porta do manager)
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo REUSEADDR do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "MANAGER: Erro ao setar o timeout do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in guest_address;     // endere�o que receber� cada participante para onde mandaremos e receberemos mensagem
    socklen_t address_len = sizeof(struct sockaddr_in);

    guest_address.sin_family = AF_INET;
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* manager_message = new packet();    // mensagem que ser� enviada para os participantes
    packet* guest_message = new packet();      // mensagem de resposta que ser� recebida dos participantes
    size_t  message_len = sizeof(packet);

    manager_message->type = SLEEP_STATUS_REQUEST;
    strcpy(manager_message->payload, STATUS_REQUEST);
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Pega a list de IPs dos participantes
        pthread_mutex_lock(&mutex_table);
        list<string> guests_list_ip_adresses = guests.returnGuestsIpAdress();
        pthread_mutex_unlock(&mutex_table);

        // Envia mensagens para todos os participantes
        for(list<string>::iterator it = guests_list_ip_adresses.begin(); it != guests_list_ip_adresses.end(); ++it)
        {
            guest_address.sin_port = htons(PORT_MONITORING_SERVICE);
            inet_aton(it->c_str(), (in_addr *) &guest_address.sin_addr.s_addr);  // envia em (porta PORT_MONITORING_SERVICE, ip do participante)

            if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0) {
                cout << "MANAGER: Erro ao enviar mensagem para algum participante." << endl;
            }

            // Esperando resposta chegar
            if (recvfrom(manager_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
            {
                // Se resposta n�o chegou, depois do timeout, atualiza status do participante para "asleep"
                pthread_mutex_lock(&mutex_table);
                guests.guestSlept(*it);
                pthread_mutex_unlock(&mutex_table);
            }
            else
            {
                // Se resposta chegou, e foi do tipo SLEEP_STATUS_REQUEST
                if (guest_message->type == SLEEP_STATUS_REQUEST)
                {
                    // Se foi avisando que est� acordado, atualiza status do participante para "awake"
                    if (strcmp(guest_message->payload, STATUS_AWAKE) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        guests.wakeGuest(*it);
                        pthread_mutex_unlock(&mutex_table);
                    }
                    // Se foi avisando que est� saindo, remove participante da tabela
                    if (strcmp(guest_message->payload, STATUS_QUIT) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        guests.deleteGuest(*it);
                        pthread_mutex_unlock(&mutex_table);
                    }
                }
            }
        }
    }
    free(manager_message);
    free(guest_message);

    close(manager_socket_descriptor);
    return nullptr;
}

// TO DO: Fica processando a entrada do usu�rio pra ver se vai vir um pedido de WAKEUP hostname -> wake on lan
void* manager_interface_service(void *arg)
{

    return nullptr;
}

// TO DO: pega o nome do host
string get_host_name()
{

}

// TO DO: pega o endere�o mac do host
string get_mac_address()
{

}

// TO DO: pega o endere�o IP do host
string get_ip_address()
{


}

string pack_discovery_sending_message()
{
    // Coleta nome do host, endere�o mac e endere�o IP
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


