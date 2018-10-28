
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <stdio.h>
#include <list>
#include <string.h>
#include <string>
#include <thread>   
#include <mutex>
#include <queue>
#include <sys/wait.h>

using namespace std;

class Paket {
    public:
        char tip;
        int posiljatelj;
        int primatelj;
        int vrijemeT;
};

class MyProces {
    public:
        int idProcesa;
        int lokalniSat;
        list<int> listaKO;
        list<Paket> zahtjevi;

};

int so;
int kolkoVasIma;
pthread_mutex_t redMutex;
queue<Paket> primljeniPaketi;
struct sockaddr_in sa;
int brojOdgovora = 0;

//otvaranje
void otvori() {
        so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (so < 0) {
                cout << "Pogreska kod stvaranja socketa" << endl;
        }
}

//pripezanje
void pripregni(int idProcesa) {
        sa.sin_family = AF_INET;
        sa.sin_port = htons(10000 + 3 * 10 + idProcesa);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(so, (struct sockaddr *) &sa, sizeof(sa)) < 0) 
        {
            cout << "Proces " << idProcesa << " - pogreska kod bindanja!";
            exit(0);
        }
}

//funkcija za slanje poruke
void posalji(int idProcesaPrimatelja, Paket &p) {
        ssize_t vel;
        sa.sin_port = htons(10000 + 3 * 10 + idProcesaPrimatelja);
        sa.sin_addr.s_addr = inet_addr("127.0.0.1");
        vel = sendto(so, &p, sizeof(p), 0, (struct sockaddr *) &sa, sizeof(sa));
        if (vel < 0) {
                cout << "Greška kod slanja poruke" << endl;
        }
        if (vel < sizeof(p)) {
                cout << "Greška kod slanja poruke" << endl;
        }
        pthread_mutex_lock(&redMutex);
        cout << "Proces "<< p.posiljatelj << " poslao poruku " << p.tip <<"("<< p.posiljatelj <<","<< p.vrijemeT <<") k procesu "<< p.primatelj << endl;
        pthread_mutex_unlock(&redMutex);
}

//funkcija za primanje poruke
void primi(MyProces &proces) {
        ssize_t vel = sizeof(sa);
        Paket p;
        while(true) {
            vel = recvfrom(so, &p, sizeof(p), 0, (struct sockaddr *) &sa, (socklen_t *) &vel);
            if (vel < 0) {
                    if (errno == EINTR) {
                            return;
                    }
                    cout << "Greska kod primanja poruke" << endl;
            }
            if (vel < sizeof(p)) {
                    cout << "Greska kod primanja poruke" << endl;
            }
            pthread_mutex_lock(&redMutex);
            {	
                cout<< "P" << proces.idProcesa << " primio poruku " << p.tip <<"("<< p.posiljatelj <<","<< p.vrijemeT <<") od " << p.posiljatelj <<endl;
                primljeniPaketi.push(p);
            }
            pthread_mutex_unlock(&redMutex);
        }

}

void makni(list<Paket>& listaPaketa, Paket &poruka, int ID)
{
	list<Paket>::iterator it = listaPaketa.begin();
	while (it != listaPaketa.end())
 	{
 		if((*it).posiljatelj == poruka.posiljatelj && (*it).vrijemeT == poruka.vrijemeT)
 		{
 			listaPaketa.erase(it++);
 			break;
 		}
 		else
 		{
 			++it;
 		}
 	}
}

void povecajLokalniSat(MyProces &proces, int satDrugogaProcesa)
{
	{
		if(proces.lokalniSat < satDrugogaProcesa)
		{
			proces.lokalniSat = satDrugogaProcesa + 1;
		}
		else
		{
			proces.lokalniSat = proces.lokalniSat + 1;
		}
	}
	cout << "T("<<proces.idProcesa<<") = " << proces.lokalniSat<< endl;
}

bool sortirajListuZahtjeva(const Paket& a, const Paket& b)
{
	if(a.vrijemeT < b.vrijemeT) 
        return true;
	if(a.vrijemeT > b.vrijemeT) 
        return false;
	if(a.vrijemeT == b.vrijemeT && a.posiljatelj < b.posiljatelj) 
        return true;
	if(a.vrijemeT == b.vrijemeT && a.posiljatelj > b.posiljatelj) 
        return false;
}

//obrada zahtjeva u nekritičnom odsječku i kritičnom odsječku
void obradaPoruka(MyProces &proces) {
    bool provjera = true;
    Paket paket;
    Paket odgovor;
    pthread_mutex_lock(&redMutex);
    {
        if(!primljeniPaketi.empty())
        {
            paket = primljeniPaketi.front();
            primljeniPaketi.pop();
            provjera = false;
        }
    }
    pthread_mutex_unlock(&redMutex);
    if (!provjera) {
            if(paket.tip == 'Z')
	        {
                proces.zahtjevi.push_back(paket);
                proces.zahtjevi.sort(sortirajListuZahtjeva);
                povecajLokalniSat(proces, paket.vrijemeT);
                odgovor.posiljatelj = proces.idProcesa;
                odgovor.primatelj = paket.posiljatelj;
                odgovor.tip = 'O';
                odgovor.vrijemeT = proces.lokalniSat;
                posalji(odgovor.primatelj,odgovor);
            }
            else if (paket.tip == 'O') {
                brojOdgovora++;
                povecajLokalniSat(proces,paket.vrijemeT);
            }
            else if (paket.tip == 'I') {
                makni(proces.zahtjevi,paket, proces.idProcesa);
            }
    }
}

//funkcija za obavljanje kritičnoga odsječka
void KO(MyProces &proces) {
    brojOdgovora = 0;
    Paket zahtjev;
    zahtjev.posiljatelj = proces.idProcesa;
    zahtjev.tip = 'Z';
    zahtjev.vrijemeT = proces.lokalniSat;
    proces.zahtjevi.push_back(zahtjev);
    proces.zahtjevi.sort(sortirajListuZahtjeva);
    for (int i = 1; i <= kolkoVasIma; i++) {
        if (i != proces.idProcesa) {
            zahtjev.primatelj = i;
            posalji(i,zahtjev);
        }
    }

    do {
        //cout << "Posiljatelj: " << brojOdgovora << endl;
        obradaPoruka(proces);
    }while((brojOdgovora < (kolkoVasIma - 1)) || !(proces.zahtjevi.front().posiljatelj == proces.idProcesa));

    sleep(3);

    makni(proces.zahtjevi,zahtjev,proces.idProcesa);

    Paket izadji;
	izadji.posiljatelj = proces.idProcesa;
	izadji.vrijemeT = zahtjev.vrijemeT;
	izadji.tip = 'I';

    for (int i = 1; i <= kolkoVasIma; i++) {
        if (i != proces.idProcesa) {
             izadji.primatelj = i;
             posalji(i,izadji);
        }
    }
}

void childProcess(MyProces &proces) {
    otvori();
    pripregni(proces.idProcesa);
    pthread_mutex_init(&redMutex,NULL);
    thread dretva(primi, std::ref(proces));
    sleep(2);
    while(true) {
        int provjeraKO = 0;
        if (!(proces.listaKO.empty()) && (proces.lokalniSat >= proces.listaKO.front())) { //lista kriticnih odsjecaka nije prazna
            provjeraKO = 1;
            KO(proces);
            proces.listaKO.pop_front();
        }

        obradaPoruka(proces);

        if (provjeraKO == 0) {
            sleep(1);
            cout << "Dogadaj("<<proces.idProcesa<<")" << endl;
            povecajLokalniSat(proces,0);
        }
    }
}

int main ( int argc, char **argv) {
    kolkoVasIma = 0;
    MyProces proces;
    int counter = 0;
    vector <MyProces> l_proces(0);
    for (int i = 1; i < argc; i++ ) {
        if (strcmp(argv[i], "@") == 0) {
            counter++;
        }
        if (counter == 0) {
            proces.idProcesa = i;
            proces.lokalniSat = atoi(argv[i]);
            l_proces.push_back(proces);
        }
        else {
            if (!strcmp(argv[i], "@") == 0) {
                l_proces[counter-1].listaKO.push_back(atoi(argv[i]));
                //cout << l_proces[counter-1].listaKO[0] << endl; //za provjeru jesu li ispravni kriticni odsjesci
            }
        }
    }

    	for (int i = 0; i < l_proces.size(); i++)
	{
        cout << l_proces[i].lokalniSat << endl;
    }

    kolkoVasIma = l_proces.size();
	for (int i = 0; i < kolkoVasIma; i++)
	{
		switch(fork())
		{
			case 0:
					childProcess(l_proces[i]);
					exit(0);
					break;
			case -1:
					i--;
					cout << "Greska kod forkanja" << endl;
					break;
		}
	}

    for (int i = 0; i< l_proces.size(); i++) {
        wait(NULL);
    }

    return 0;
}