#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include <mutex> // ���뻥����ͷ�ļ�
#include<string>
#include<thread>
#include <unordered_map>
#include <vector>
#include <map>
#include <set>
#pragma comment (lib,"ws2_32.lib")
using namespace std;
sockaddr_in serverAddr;
sockaddr_in clientAddr;
SOCKET  serverSock, clientSock;
int clientAddrsizef = sizeof(clientAddr);
bool pan = false;
int clientCount = 0; // �ͻ�������������
std::vector<SOCKET> clientSocks; // �洢���пͻ����׽��ֵ�����
std::mutex mtx;

void accept();//�������ӿͻ���
void allocation();//��������socket��ip�Ͷ˿�
void SendThread(SOCKET clientSock);//����ͻ��˷��͵ĵ��߳�
void xiancient(SOCKET clientSock, sockaddr_in  clientAddr);//������ͻ��˽�����Ϣ��������Ⱥ�ĺ�˽�ĵ��߳�

int main(){
	allocation();
	while (!pan) { 
		accept();
		thread  clientThread(xiancient,clientSock, clientAddr);
		thread sendThread(SendThread, clientSock);
		// �����̣߳����߳���������
		clientThread.detach();
		sendThread.detach();
		std::cout << "����esc�˳���"<<"query��ѯ" << endl;
	}
	closesocket(serverSock);
	WSACleanup();
	return 0;
}

void xiancient(SOCKET clientSock, sockaddr_in  clientAddr) {//������ͻ��˽�����Ϣ��������Ⱥ�ĺ�˽�ĵ��߳�
	string shuru;
	char clientbuf[4096] ;//����
	int bytesReceived = 0;
	char clientIP[INET_ADDRSTRLEN];
	string clientIP_1[10];//ip����
	int clientPort_1[10];//�˿�����
	inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
	
	unsigned short clientPort = ntohs(clientAddr.sin_port);
	{
		std::cout << "�ͻ��� " << clientIP << ":" << clientPort << " ������" << endl;
		std::lock_guard<std::mutex> guard(mtx); // �ڷ��ʹ�����Դǰ����������
		clientIP_1[clientCount] = clientIP;
		clientPort_1[clientCount] = clientPort;
	}
	cout << ++clientCount << endl;
	while (!pan) {
		memset(clientbuf, 0, sizeof(clientbuf));// ���տͻ�����Ϣ
		bytesReceived = recv(clientSock, clientbuf, sizeof(clientbuf), 0);
		if (bytesReceived > 0) {
			 //������Ϣ��ʽΪ "P|receiverId|message" �� "G|groupId|message"
			std::string msg(clientbuf, bytesReceived);
			if (msg[0] == 'P') { // ˽����Ϣ
				// ������Ϣ���ҵ�Ŀ�������
				char shibuf[1024] = "����˽��";
				send(clientSock, shibuf, sizeof(shibuf), 0);

				size_t separatorPos = msg.find('|', 1);
				if (separatorPos != std::string::npos) {
					std::string targetClientIdStr = msg.substr(1, separatorPos - 1);
					std::string actualMessage = msg.substr(separatorPos + 1);
					int targetClientId = std::stoi(targetClientIdStr) - 1; // ת��Ϊ0-based����
					if (targetClientId >= 0 && targetClientId < clientSocks.size()) {
						// �ҵ�Ŀ��ͻ����׽���
						SOCKET targetSock = clientSocks[targetClientId];
						// ��Ŀ��ͻ��˷���ʵ����Ϣ
						send(targetSock, actualMessage.c_str(), actualMessage.length(), 0);
						std::cout << "��Ϣ�ѷ������ͻ���ID " << targetClientId + 1 << std::endl;
					}else {
						std::cout << "��Ч�Ŀͻ���ID: " << targetClientId + 1 << std::endl;
					}
				}else {
					std::cout << "��Ч��˽����Ϣ��ʽ��" << std::endl;
				}	
			}else if (msg[0] == 'G') { // Ⱥ����Ϣ
				char qunbuf[1024] = "����Ⱥ��";
				send(clientSock, qunbuf, sizeof(qunbuf), 0);
				memset(clientbuf, 0, sizeof(clientbuf));// ���տͻ�����Ϣ
				std::cout << clientIP << ":" << clientPort << " ����Ⱥ����Ϣ��" << msg.substr(1) << endl;
				clientSocks.push_back(clientSock); // ���¿ͻ����׽��ּ����б�
				auto messageContent = msg.substr(1);
				
				for (SOCKET& c : clientSocks) {
					send(c, msg.substr(1).c_str(), msg.size() - 1, 0);
				}
			}else if(msg[0]!=('G' || 'P')) {
				std::cout << clientIP << ":" << clientPort << " ���յ���Ϣ��" << clientbuf << endl;
				send(clientSock, clientbuf, sizeof(clientbuf), 0);
			}
		}else if (bytesReceived == 0) {
			std::cout << clientIP << ":" << clientPort << " �ͻ��˶Ͽ����ӣ�" << endl;
			{
				std::lock_guard<std::mutex> guard(mtx); // ���޸Ĺ�����Դǰ����������
				cout << --clientCount << endl;// ÿ���пͻ��˶Ͽ�����ʱ�����ټ�����
			}
			break;
		}else {
			std::cout << "����ʱ����" << endl;
			{
				std::lock_guard<std::mutex> guard(mtx); // ���޸Ĺ�����Դǰ����������
				cout << --clientCount << endl;
			}
			break;
		}
	}
	closesocket(clientSock);
}


void allocation() {//��������socket��ip�Ͷ˿�
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "��ʼ��socket��ʧ�ܣ�" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "��ʼ��socket��ɹ���" << endl;
	}
	if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		std::cout << "�׽�socketʧ��" << endl;
		WSACleanup();
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cout << "bindʧ��" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "�󶨳ɹ���" << endl;
	}
	if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "����ʧ��" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "�����ɹ���" << endl;
	}
}

void SendThread(SOCKET clientSock) {//����ͻ��˷��͵ĵ��߳�
	clientSocks.push_back(clientSock); // ���¿ͻ����׽��ּ����б�	
	while (!pan) {
		string shur;
		cin >> shur;
		if (shur == "esc") {
			cout << "ǿ���˳�" << clientCount << endl;
			pan = true;
			//closesocket(serverSock);
			exit(0);
			break;
		}
		if (shur == "query") {
			lock_guard<std::mutex> lock(mtx);
			cout << "��ǰ�ͻ���������" << clientCount << endl;
		} 
			for (SOCKET& c : clientSocks) {
				send(c, shur.substr().c_str(), shur.size(), 0);
			}
		}
	}


void accept() {//�������ӿͻ���
	clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientAddrsizef);
	if (clientSock == INVALID_SOCKET) {
		std::cout << "δ�ܽ��տͻ�������" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "���տͻ������ӳɹ���" << endl;
	}
}

