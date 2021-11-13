# ISA projekt

**Autor:** Jakub Šuráň
**Login:** xsuran07
**Datum:** 13.11. 2021

## Popis

*mytftpclient* představuje implementaci jednoduchého TFTP klienta. Je zde použit protokol TFTPv2 (RFC 1350) - mj.
oprava syndromu čarodějova učně (Sourcerer's Apprentice Syndrome). Kromě toho tento klient podporuje i rozšiřující
možnosti, jak byly definovány v rámci RFC 2347. Konkrétně se jedná o rozšíření blksize (možnost vyjednat se
serverem velikost datového bloku - RFC 2348), timeout (možnost vyjednat délku timeoutu před znovuposláním na
straně serveru - RFC 2349) a tsize (specifikace velikosti přenášeného souboru - RFC 2349). Naopak nebylo v žádné míře
implementováno rozšíření multicast (RFC 2090).

Klient funguje tak, že po spuštění je uživateli nabídnut interaktivní terminál, kam může zadávat příkazy (popis
příkazů viz dále). Jelikož je klient implementován nad UDP, je v implementaci využito timeoutů. Jeden z nich
je použit k zajištění toho, aby klient při dlouhém čekání na odpověď znovuposlal poslední paket (který se např. ztratil v síti).
Další timeout hlídá maximální dobu, po kterou je klient ochoten čekat na odpověď - pokud vyprší, je komunikace ukončena
jako neúspěšná.

Uživatel je průběžně informován o průběhu TFTP komunikace se serverem - časové razítka odeslaných a přijatých paketů +
rozbor jejich obsahu. Na konci každého přenosu je vypsána informace, zda se přenos podařilo dokončit bez chyb nebo ne.

Při vyjednávání o rozšiřujících možnostech se počítá s tím, že je serveru nemusí podporovat. Pokud server rozšíření
vůbec nepodporuje a v úvodním požadavku je naprosto ignoruje, *mytftpclient* je schopen toto akceptovat a bez problémů
pokračovat v komunikaci. Navíc platí, že pokud server na poždavek s rozšířeními odpoví ERROR paketem s chybou číslo 8
(chyba v rozšiřujících možnostech), *mytftpclient* komunikaci neukončí, nýbrž modifikuje úvodní požadavek a pošle jej znovu.
Modifikace úvodního poždadavku spočívá v tom, že se z něj odstraní jedno z rozšíření. Rozhodnutí, které rozšíření se má odtranit,
probíhá podle tohoto klíče. Jestliže problematický požadavek obsahoval rozšíření tsize, odstraní se a požadavek se pošle znova.
V opačném případě se zkoumá, zda požadavek obsahoval rozšíření timeout. Pokud ano, odstraní se a se pošle modifikovaný požadavek.
Pokud ani toto rozšíření nebylo přítomno, je odstraněno rozšíření blksize (je-li přítomno). Díky tomu takto modifikovaný požadavek
v tuto chvíli zaručeně neobsahuje žádné rozšíření. Pokud by rozšíření blksize v paketu nebylo přítomno, jedná se
o zjevnou chybu ze strany serveru (v požadavku nebylo žádné rozšíření, ale přesto se server tváří, že je v nich chyba).
V takovém případě je komunikace ukončena s chybou.

## Použití

Kompilace a spuštění aplikace:
```bash
make
./mytftpclient
```
Po spuštění aplikace je uživateli k dispozici interaktivní terminál, kam je možné zadávat tyto příkazy:
- help - vypsání nápovědy s přehledem a popisem dostupných příkazů a jejich parametrů
- quit - ukončení interaktivního terminálu (stejný efekt má i zadání EOF, např. na linuxu pomocí ctrl+D)
- {TFTP požadavek} - vyžádání si TFTP požadavku se specifikovanými parametry

Příkaz {TFTP požadavek} je nutné specifikovat pomocí těchto parametrů:
- -R nebo -W (povinný) - specifikace, zda se má jednat o čtění nebo zápis na server (je nutné uvést právě jeden z těchto přepínačů)
- -d *cesta/soubor* (povinný) - specifikace přenášeného souboru; *cesta* musí být absolutní cesta udávající, kam se má na
serveru uložit přenášený soubor, resp. odkud vzít; *soubor* udává název přenášeného souboru; na straně klienta se soubor pri
čtení uloží do aktulního lokálního adresáře a při zápisu se soubor také hledá v aktuálním lokálním adresáři
- -t *timeout* (nepovinný) - *timeout* udává hodnotu v sekundách, kterou klient bude navrhovat serveru v rámci rozšíření timeout
(RFC 2349); akceptovány jsou hodnoty z intervalu 1 - 255 (včetně)
- -s *velikost* (nepovinný) - *velikost* udává hodnotu v bajtech, kterou klient bude navrhovat serveru v rámci rozšíření blksize
(RFC 2348); akceptovány jsou hodnoty z intervalu 8 - 65464 (včetně); pokud není uveden, implicitně se uvažuje velikost datového bloku 512 bajtů
- -c *mód* (nepovinný) - *mód* udává přenosový mód; akceptovány jsou hodnoty "ascii" (nebo "netascii") a "binary" (nebo "octet");
pokud není uveden, implicitně se uvažuje hodnota "binary"
- -a *adresa, port* (nepovinný) - *adresa* specifikuje adresu serveru - podporovány jsou ipv4 i ipv6 adresy; *port* udává číslo
portu, na kterém server naslouchá; pokud není uveden, implicitně se uvažuje adresa 127.0.0.1 (ipv4 localhost) a číslo port 69
- -m (nepovinný) - vyžádání si přenosu skrze multicastu; je možné použít, ale je bez efektu - toto rozšíření není implementováno

## Příklady spuštění

Aplikace se spouští tímto přikazem:
```bash
./mytftpclient
```
Následují ukázky příkazů, které je možné zadat do terminálu:
| Příkaz                              | Krátký popis                                                                                  |
|-------------------------------------|-----------------------------------------------------------------------------------------------|
| >help                               | Vypíše nápovědu k možným příkazům                                                             |
| >quit                               | Ukončí interaktivní terminál                                                                  |
| >-R -d /tftp/test.txt               | Vyžádání si čtení souboru test.txt, který je na serveru uložen v adresáři /tftp               |
| >-R -d /tftp/test.txt -c ascii      | Vyžádání si čtení souboru test.txt, kdy módem přenosu bude "ascii"                            |
| >-W -d /tftp/test.txt -t 30 -s 1024 | Vyžádání si zápisu souboru test.txt, kdy serveru bude navrhnuta nová velikost bloku a timeout |
| >-W -d /tftp/test.txt -a ::1, 69    | Vyžádání si zápisu souboru test.txt, kdy je specifikována adresa serveru                      |
| >-d /tftp/test.txt                  | Chyba! Není specifikováno, zda se jendá o zápis nebo čtení                                    |
| >-R -d tftp/test.txt                | Chyba! U přepínače -d je vyžadována absolutní cesta                                           |
| >-R -d /tftp/test.txt -t 500        | Chyba! Hodnota u přepínače -t musí být z intervau 1 - 255 (včetně)                            |
| >-R -d /tftp/test.txt -s 4          | Chyba! Hodnota u přepínače -s musí být z intervau 8 - 65464 (včetně)                          |

## Odevzdané soubory

| Název souboru       | Popis                                                                       |
|---------------------|-----------------------------------------------------------------------------|
| Makefile            | Makefile sloužící ke kompilaci a sestavení celého projektu                  |
| manual.pdf          | Krátká dokumentace celého projektu                                          |
| mytftpclient.cpp    | hlavní soubor s funkcí main                                                 |
| parser.cpp          | Implementace třídy zajišťující parsování příkazů v teminálu                 |
| parser.h            | Rozhraní třídy zajišťující parsování příkazů v teminálu                     |
| README.md           | Tento soubor - krátký popis projektu                                        |
| terminal.cpp        | Implementace třídy zajišťující vytvoření a obsluhu interaktivního terminálu |
| terminal.h          | Rozhraní třídy zajišťující vytvoření a obsluhu interaktivního terminálu     |
| tftp_client.cpp     | Implementace třídy zajišťující vlastní TFTP komunikaci                      |
| tftp_client.h       | Rozhraní třídy zajišťující vlastní TFTP komunikaci                          |
| tftp_parameters.cpp | Implementace třídy zajišťující parsování parametrů TFTP požadavku           |
| tftp_parameters.h   | Rozhraní třídy zajišťující parsování parametrů TFTP požadavku               |