/*
   Projekt: Wordclock
   Version: 1.00
   Autor: SPSundTechnik

   Info:
   Unter dem Punkt "Parameter" SSID und Passwort des verwendeten WLAN's eintragen.
   Die Bedienung per App wurde deaktiviert. Farbwerte für die LED's müssen in den
   Parametern eingestellt werden.

   Berndeutsche Version: Simon Flückiger
   Datum: 27.08.2020
*/


//Bibliotheken
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>


//Anzahl LED's und Ausgang vom ESP festlegen
#define PIN         D6
#define NUMBERS     121


//*********************************************** PARAMETER ************************************************

//Zugangsdaten WLAN
//char ssid[] = "WeAre";    //SSID
//char pass[] = "nothing!";    //Passwort


//Lokale Portnummer für das UDP-Paket
const unsigned int localPort = 2390;


//Lokale Portnummer für HTTP
const unsigned int http = 80;


//Adresse des NTP-Serverpools
const char* ntpservername = "de.pool.ntp.org";


//Größe des NTP-Datenpaket
const unsigned int ntppaket = 48;
byte paketpuffer[ntppaket];


//Zeitdaten
const unsigned long secondperday = 86400;
const unsigned long secondperhour = 3600;
const unsigned long secondperminute = 60;


//Anzahl der Tage im Monat
unsigned int tageImMonat[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


//Anzahl der Schalttage seit dem jahr 1900 bis Baujahr xy
//Baujahr der Uhr: 2018
//Schalttage von 1900 bis 2018 = 28
// ---> TODO: 1900 bis 2020 = 29??? stimmt das?
unsigned int newschaltTage = 30;


//Zeitintervalle in Millisekunden
const unsigned long intervallNTP = 10000;
const unsigned long intervallDebug = 10000;


//Adresse der ersten und letzten LED
const int ersteLED = 0;
const int letzteLED = 120;


//Maximale und minimale Helligkeit der LED's
//Wird nur bei Bedienung per App verwendet!
const int maxBrightLED = 100;
const int minBrightLED = 0;


/************************************ Farbwerte und Helligkeit der LED's *************************************/

//Beispiel: RGB-Farbschema Rot = 50; Grün = 50; Blau = 50 => ergibt einen weißen Farbton
//Tipp: Der blaue Anteil muss bei den WS2812B-LED's etwas reduziert werden, da dieser dominanter ausfällt!
//Helligkeit der LED's kann mit Werten zwischen 0 - 255 eingestellt werden.
//Beispiel 1: Rot = 255; Grün = 255; Blau = 255 => weißer Farbton mit maximaler Helligkeit aller LED's!
//Beispiel 2: Rot = 0; Grün = 0; Blau = 255 => blauer Farbton mir maximaler Helligkeit der blauen LED

const int redValueLED = 50;
const int greenValueLED = 50;
const int blueValueLED = 30;

/************************************************************************************************************/


//Adressen der Stundenzahlen
const int STUNDENADR[12][2] = { {110, 115},   //Zwöufi
  {51, 53},     //Eis
  {62, 65},     //Zwöi
  {58, 60},     //Drü
  {67, 71},     //Vieri
  {73, 76},     //Füfi
  {82, 87},     //Sächsi
  {77, 81},     //Sibni
  {88, 92},     //Achti
  {94, 97},     //Nüni
  {106, 109},    //Zäni
  {99, 102}    //Eufi
};

//Adressen der Minutenzahlen
const int MINUTENADR[9][2] = { {19, 21},      //Füf
  {30, 32},      //Zää
  {11, 16},      //Viertu
  {22, 27},      //Zwänzg
  {44, 48},      //Haubi
  {37, 38},      //ab
  {41, 43},      //vor
  {8, 10},      //gli
  {117, 119}    //gsi
};
//*************************************************************************************************************


//************************************************* VARIABLEN *************************************************

//Zähler for-Schleifen
int i = 0;
int z = 0;
int x = 0;
int y = 0;

unsigned int nptpaketgroesse = 0;
unsigned int fehlerpaketgroesse = 0;

unsigned long highWord = 0;
unsigned long lowWord = 0;

unsigned long newsecsSince1900 = 0;
unsigned long oldsecsSince1900 = 0;

unsigned int oldstunde = 99;  //Damit beim Neustart die Stundenzahl aktualisiert wird!
unsigned int oldminute = 0;

unsigned int utcaktstunde = 0;
unsigned int utcaktminute = 0;
unsigned int utcaktsekunde = 0;

unsigned int mezaktstunde = 0;
unsigned int mezaktminute = 0;
unsigned int mezaktsekunde = 0;

unsigned long newtageSeit1900 = 0;
unsigned long oldtageSeit1900 = 0;
unsigned long hourssince1900 = 0;

unsigned int wochenTag = 0;

unsigned int datumTag = 0;
unsigned int datumMonat = 0;
unsigned int datumJahr = 0;

unsigned int tagNummer = 0;

unsigned int utcx = 0;

unsigned int oldschaltTage = 0;
bool schaltJahr = LOW;
bool sonderFall366 = LOW;

unsigned long lastmillisNTP = 0;
unsigned long lastmillisDebug = 0;

int positionen[20];
char zeichen = '0';

String puffer = "S:0:0:0:0:0:0:E";
String substring_1 = "0";
String substring_2 = "0";
String substring_3 = "0";
String substring_4 = "0";

unsigned int wert_1 = 0;
unsigned int wert_2 = 0;
unsigned int wert_3 = 0;
unsigned int wert_4 = 0;

int red = 0;
int green = 0;
int blue = 0;

int startadresse = 0;
int endadresse = 0;

int pointer = 0;
int oldpointer = 0;

int newletter[120];
int showletter[120];
bool newdisplay = LOW;

bool debug = LOW;  //Bei HIGH => Öffnet serielle Schnittstelle fürs Debuging

//****************************************************************************************************************


//Objekte anlegen
IPAddress timeServerIP;

WiFiUDP udp;

WiFiServer server(http);

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMBERS, PIN, NEO_GRB + NEO_KHZ800);


//Eigene Funktionen
unsigned int wochenTag_calc(unsigned long);
bool schaltJahrJN(unsigned int);
int welcherMonat(int, bool);


//SETUP
void setup() {


  //Serielle Schnittstelle für Debuging öffen
  if (debug == HIGH) {
    Serial.begin(9600);
  }


  //Leds initialisieren
  pixels.begin();

  //Array auf null setzen
  for (i = ersteLED; i <= letzteLED; i++) {

    newletter[i] = 0;
  }

  //Array auf null setzen
  for (i = ersteLED; i <= letzteLED; i++) {

    showletter[i] = 0;
  }


  //Start: Aufbau der WLAN-Verbindung

  WiFiManager wifiManager;
  //wifiManager.resetSettings();

  red = 60;
  green = 0;
  blue = 0;

  pixels.setPixelColor(28, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(29, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(33, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(34, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(35, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(36, pixels.Color(red, green, blue));
  pixels.show();

  wifiManager.autoConnect("Wortuhr", "wortuhr1");

  red = 0;
  green = 0;
  blue = 0;

  pixels.setPixelColor(28, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(29, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(33, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(34, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(35, pixels.Color(red, green, blue));
  pixels.show();
  pixels.setPixelColor(36, pixels.Color(red, green, blue));
  pixels.show();

  //Ende: Aufbau der WLAN-Verbindung


  //UDP-Protokoll
  udp.begin(localPort);

  //Webserver
  server.begin();


  //Start: Test alle LEDs bei Neustart der Uhr

  //randomtest
  Serial.begin(9600);

  int leds[120];
  int ledsLeft = 120;

  for (int i = 0; i < 120; i++) {
    leds[i] = i + 1;
  }
  
  
  for (int i = 0; i < 120; i++) {
    if (ledsLeft > 0) {
      red = redValueLED;
      green = greenValueLED;
      blue = blueValueLED;

      randomSeed(analogRead(A0));
      int r = random(0, ledsLeft);
      pixels.setPixelColor(leds[r], pixels.Color(red, green, blue));
      Serial.print("TEST: ");
      Serial.println(leds[r]);
      pixels.show();
      delay(10);
      leds[r] = leds[--ledsLeft];
    }
  }

  delay(1000);

  leds[120];
  ledsLeft = 120;
  
  for (int i = 0; i < 120; i++) {
    leds[i] = i + 1;
  }
  
  for (int i = 0; i < 120; i++) {
    if (ledsLeft > 0) {
      red = 0;
      green = 0;
      blue = 0;

      randomSeed(analogRead(A0));
      int r = random(0, ledsLeft);
      pixels.setPixelColor(leds[r], pixels.Color(red, green, blue));
      Serial.print("TEST: ");
      Serial.println(leds[r]);
      pixels.show();
      delay(10);
      leds[r] = leds[--ledsLeft];
    }
  }

  delay(1000);

  //Testdurchlauf => alle LEDs nacheinander einschalten
  for (i = ersteLED; i <= letzteLED; i++) {

    red = redValueLED;
    green = greenValueLED;
    blue = blueValueLED;

    pixels.setPixelColor(i, pixels.Color(red, green, blue));
    pixels.show();
    delay(40);
  }

  //Wartezeit 1 Sekunden
  delay(1000);

  //Testdurchlauf => alle LEDs umgekehrt ausschalten
  for (i = letzteLED; i >= ersteLED; i--) {

    red = 0;
    green = 0;
    blue = 0;

    //Wörter "ES" und "ISCH" schreiben
    if (i == 0 || i == 1 || i == 3 || i == 4 || i == 5 || i == 6) {

      //"ES"
      newletter[0] = 1;
      newletter[1] = 1;

      //"ISCH"
      newletter[3] = 1;
      newletter[4] = 1;
      newletter[5] = 1;
      newletter[6] = 6;

      red = 50;
      green = 50;
      blue = 30;
    }

    pixels.setPixelColor(i, pixels.Color(red, green, blue));
    pixels.show();
    delay(40);

  }

  //Ende: Test alle LEDs bei Neustart der Uhr


  //Startwerte für die LED's
  red = redValueLED;
  green = greenValueLED;
  blue = blueValueLED;

}


//LOOP
void loop() {


  //Alle 10 sek. eine Verbindung zum NTP-Server aufbauen
  if (millis() - lastmillisNTP > intervallNTP)
  {
    Serial.println("Start NTP-Anfrage");

    //Systemzeit speichern für den nächsten Vergleich
    lastmillisNTP += intervallNTP;


    //Start: NTP-Anfrage und NTP-Antwort

    //Verbindung zu einem NTP-Server aus dem Pool aufbauen
    WiFi.hostByName(ntpservername, timeServerIP);

    //NTP-Anfrage an den Server stellen
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server

    //Wartezeit 500 ms um Antwort zu empfangen
    delay(500);

    //Paketgröße ermitteln
    nptpaketgroesse = udp.parsePacket();

    //Kontrolle die Paketgröße zum NTP-Paket (48 Byte) passt
    //wenn nicht, dann erneut 500 ms warten und wieder Paketgröße ermitteln
    if (nptpaketgroesse != ntppaket) {
      delay(500);
      nptpaketgroesse = udp.parsePacket();
    }

    //wenn Ja, dann Paket abspeichern
    if (nptpaketgroesse == ntppaket) {

      udp.read(paketpuffer, ntppaket);

      //Der Zeitstempel befindet sich im Byte 40, 41, 42, 43
      highWord = word(paketpuffer[40], paketpuffer[41]);
      lowWord = word(paketpuffer[42], paketpuffer[43]);

      //Zeitstempel aus den vier Bytes auslesen => Ergebnis: Sekunden seit dem 1. Januar 1900, 00:00:00 Uhr
      newsecsSince1900 = highWord << 16 | lowWord;

      nptpaketgroesse = 0;
      fehlerpaketgroesse = 0;
    }

    //Konnte nach zwei Versuchen keine korrekte Paketgröße ermittelt werden, dann wird der Fehlerzähler inkrementiert
    else {
      fehlerpaketgroesse = fehlerpaketgroesse + 1;
      nptpaketgroesse = 0;
    }

    //Wifi-Verbindung nach zehn fehlerhaften Veruschen unterbrechen und erneut aufbauen
    if (fehlerpaketgroesse >= 10) {
      WiFi.reconnect();

      while (WiFi.status() != WL_CONNECTED)
      {
        delay(200);
      }

      fehlerpaketgroesse = 0;
    }

    Serial.println("Ende NTP-Anfrage");

    //Ende: NTP-Anfrage und NTP-Antwort

  }



  /*
   * **************************************************************************************************************

     Die Funktion "Bedienung per App" wurde deaktiviert! Zur Aktivierung Kommentarfunktion löschen.

   * **************************************************************************************************************
  */


  //  //Start: Farbeinstellungen über die App beziehen
  //
  //  //Anfrage an den Webserver ob ein Client verfügbar ist
  //  WiFiClient client = server.available();
  //
  //  if(client) {
  //    if(client.connected()){
  //      //gesendete Daten der App in den Puffer laden
  //      puffer = client.readStringUntil('\n');
  //
  //      Serial.println(puffer);
  //
  //      client.stop();
  //    }
  //
  //    // Beispiel für Paketaufbau
  //    // GET /:0:37:50:40:e/ HTTP/1.1
  //    // 1. Wert => Display On/Off    (0/1)
  //    // 2. Wert => RGB-Farbwert Rot  (0-100)
  //    // 3. Wert => RGB-Farbwert Grün (0-100)
  //    // 4. Wert => RGB-Farbwert Blau (0-100)
  //
  //
  //    //Start: Farbwerte RGB aus dem Datenpaket filtern
  //
  //    //Sendepuffer auf Trennzeichen kontrollieren
  //    for(x = 0; x <= 50; x++){
  //
  //      zeichen = puffer.charAt(x);
  //
  //      //Kontrolle ob Trennzeichen vorliegt
  //      if(zeichen == ':') {
  //
  //        //Position im Datenpakt speichern
  //        positionen[z] = x;
  //        z++;
  //      }
  //
  //      //Kontrolle ob Endezeichen vorliegt
  //      if(zeichen == 'e') {
  //        z = 0;
  //        break;
  //      }
  //    }
  //
  //    //Farbwerte an den bestimmten Positionen herausfiltern
  //    substring_1 = puffer.substring(positionen[0] + 1, positionen[1]);
  //    substring_2 = puffer.substring(positionen[1] + 1, positionen[2]);
  //    substring_3 = puffer.substring(positionen[2] + 1, positionen[3]);
  //    substring_4 = puffer.substring(positionen[3] + 1, positionen[4]);
  //
  //    //Sendepuffer leeren
  //    puffer = "";
  //
  //    //Convert String to Int
  //    wert_1 = substring_1.toInt();
  //    wert_2 = substring_2.toInt();
  //    wert_3 = substring_3.toInt();
  //    wert_4 = substring_4.toInt();
  //
  //    //Ende: Farbwerte RGB aus dem Datenpaket filtern
  //
  //
  //    //Start: Unter- und Obergrenze für empfangene Werte festlegen
  //
  //    //Untergrenzen kontrollieren
  //    if(wert_2 < minBrightLED) {
  //      wert_2 = minBrightLED;
  //    }
  //
  //    if(wert_3 < minBrightLED) {
  //      wert_3 = minBrightLED;
  //    }
  //
  //    if(wert_4 < minBrightLED) {
  //      wert_4 = minBrightLED;
  //    }
  //
  //    //Obergenzen kontrollieren
  //    if(wert_2 > maxBrightLED) {
  //      wert_2 = maxBrightLED;
  //    }
  //
  //    if(wert_3 > maxBrightLED) {
  //      wert_3 = maxBrightLED;
  //    }
  //
  //    if(wert_4 > maxBrightLED) {
  //      wert_4 = maxBrightLED;
  //    }
  //
  //    //Ende: Unter- und Obergrenze für empfangene Werte festlegen
  //
  //
  //    //Display eingeschaltet => wert_1 == high
  //    if(wert_1 == 1) {
  //      red = wert_2;
  //      green = wert_3;
  //      blue = wert_4;
  //    }
  //
  //    //Display ausgeschaltet => wert_1 == low
  //    if(wert_1 == 0) {
  //      red = minBrightLED;
  //      green = minBrightLED;
  //      blue = minBrightLED;
  //    }
  //
  //    //Display aktualisieren
  //    newdisplay = HIGH;
  //
  //  }
  //
  //  //Ende: Farbeinstellung über die App beziehen


  /*
   * **************************************************************************************************************

     Die Funktion "Bedienung per App" wurde deaktiviert! Zur Aktivierung Kommentarfunktion löschen.

   * **************************************************************************************************************
  */


  //Start: Datum und Uhrzeit ermitteln

  //Kontrolle ob eine Änderung bei dem Zeitstempel stattgefunden hat
  if (newsecsSince1900 > oldsecsSince1900) {

    //Änderung abspeichern für den nächsten Vergleich
    oldsecsSince1900 = newsecsSince1900;

    //Tage seit dem 1. Januar 1900; 00:00:00 Uhr
    newtageSeit1900 = newsecsSince1900 / secondperday;



    //Start: Datum ermitteln

    //Kontrolle ob eine Änderung bei den Tagen seit dem 1. Januar 1900 stattgefunden hat
    if (newtageSeit1900 > oldtageSeit1900) {

      //Änderung abspeichern für den nächsten Vergleich
      oldtageSeit1900 = newtageSeit1900;


      //Aufruf der Funktion zur Berechnung des Wochentages
      wochenTag = wochenTag_calc(newtageSeit1900);


      //Berechnug der Jahreszahl
      datumJahr = ((newtageSeit1900 - newschaltTage) / 365.002f) + 1900;

      //Aufruf der Funktion zur Erkennung ob ein Schaltjahr vorliegt
      schaltJahr = schaltJahrJN(datumJahr);

      //Funktion fürs Schaltjahr
      if (schaltJahr == HIGH) {

        tageImMonat[2] = 29;


        //Anzahl der Schalttage seit dem 1. Januar 1900 !ERST! am Ende des jeweiligen Schaltjahres hochzählen
        //Sonderfall: Zur Berechnung des 366. Tages wird der "alte" Wert der Schalttage benötigt, die Berechnung
        //            der Jahreszahl am 366. Tag muss aber mit dem neuen Wert durchgeführt werden

        //Berechnung der Anzahl der Schalttage seit dem 1. Januar 1900
        if (datumMonat == 12 && datumTag == 30) {

          oldschaltTage = newschaltTage;

          newschaltTage = newschaltTage + 1;

          sonderFall366 = HIGH;
        }
      }

      else {

        tageImMonat[2] = 28;
      }


      //Berechnung der wieviele Tag im Jahr
      //Sonderfall: Die Berechnung des 366. Tages im Schaltjahr muss mit dem "alten" Wert der Schalttage (seit 1900) berechnet werden
      if (sonderFall366 == HIGH) {

        tagNummer = (newtageSeit1900 - ((datumJahr - 1900) * 365) - oldschaltTage);

        sonderFall366 = LOW;
      }

      //Ansonsten muss mit dem aktuellen Wert der Schalttage (seit 1900) gerechnet werden
      else {

        tagNummer = (newtageSeit1900 - ((datumJahr - 1900) * 365) - newschaltTage);
      }

      //Aufruf der Funktion zur Berechnung des Monates
      datumMonat = welcherMonat(tagNummer, schaltJahr);

      datumTag = 0;

      //Berechnung der bereits vergangenen Tagen im Jahr
      for (i = 0; i < datumMonat; i++) {

        datumTag = datumTag + tageImMonat[i];
      }

      //Tag-Nummer minus bereits vergangener Tage im Jahr = aktueller Tag
      datumTag = tagNummer - datumTag;

    }

    //Ende: Datum ermitteln



    //Start: Sommer-/ Winterzeit einstellen

    //am letzten Sonntag im März(03) wird von UTC+1 auf UTC+2 umgestellt; 02:00 -> 03:00
    //am letzten Sonntag im Oktober(10) wird von UTC+2 auf UTC+1 umgestellt; 03:00 -> 02:00

    //Aufruf jeden Sonntag im März
    if (datumMonat == 3 && wochenTag == 7) {

      //Erkennung ob letzter Sonntag im März
      if ((tageImMonat[3] - datumTag) < 7) {

        //Umstellung auf Sommerzeit UTC+2
        utcx = 2;
      }
    }

    //Aufruf jeden Sonntag im Oktober
    if (datumMonat == 10 && wochenTag == 7) {

      //Erkennung letzter Sonntag im Oktober
      if ((tageImMonat[10] - datumTag) < 7) {

        //Umstellung auf Winterzeit UTC+1
        utcx = 1;
      }
    }

    //Falls Neustart der Uhr innerhalb der Sommerzeit
    if ((datumMonat > 3 && datumMonat <= 10) && utcx == 0) {
      utcx = 2;
    }

    //Falls Neustart der Uhr innerhalb der Winterzeit
    if ((datumMonat <= 3 || datumMonat > 10) && utcx == 0) {
      utcx = 1;
    }

    //End: Sommer-/ Winterzeit einstellen



    //Berechnung der aktuellen Stunde
    utcaktstunde = ((newsecsSince1900  % secondperday) / secondperhour);

    //Berechnung der aktuellen Minute
    utcaktminute = (newsecsSince1900  % secondperhour) / secondperminute;

    //Berechnung der aktuellen Sekunde
    utcaktsekunde = (newsecsSince1900 % secondperminute);

  }

  //Ende: Datum und Uhrzeit ermitteln



  //Start: Von UTC nach MEZ

  //Sekunden und Minuten unverändert von UTC übernehmen
  mezaktminute = utcaktminute;
  mezaktsekunde = utcaktsekunde;

  //Umrechnung von UTC nach MEZ mittels Zeitverschiebung
  mezaktstunde = utcaktstunde + utcx;


  //Nach "Halb" Stundenzahl erhöhen => Beispiel: 16:25:00 => Fünf vor halb Fünf
  if (mezaktminute >= 23 && mezaktminute <= 59) {

    mezaktstunde = mezaktstunde + 1;
  }


  //Beispiel: UTC             = 22:15:00
  //          MEZ (Winter +1) = 23:15:00 => 23:15:00 => 11:15:00 => viertel nach Elf
  //          MEZ (Sommer +2) = 24:15:00 => 00:15:00 => viertel nach Zwölf

  //Beispiel: UTC             = 23:15:00
  //          MEZ (Winter +1) = 24:15:00 => 00:15:00 => viertel nach Zwölf
  //          MEZ (Sommer +2) = 25:15:00 => 01:15:00 => viertel nach Eins

  //Problemstellung 1: Stundenzahlen über 23 abfangen
  if (mezaktstunde >= 24) {
    mezaktstunde = mezaktstunde - 24;
  }


  //Beispiel: UTC             = 10:47:00
  //          MEZ (Winter +1) = 11:15:00 => 11:15:00 => viertel nach Elf
  //          MEZ (Sommer +2) = 12:15:00 => 00:15:00 => viertel nach Zwölf

  //Beispiel: UTC             = 11:47:00
  //          MEZ (Winter +1) = 12:15:00 => 00:15:00 => viertel nach Zwölf
  //          MEZ (Sommer +2) = 13:15:00 => 01:15:00 => viertel nach Eins

  //Problemstellung 2: Stundenzahlen auf den Bereich "Eins" bis "Zwölf" begrenzen
  if (mezaktstunde >= 12) {
    mezaktstunde = mezaktstunde - 12;
  }

  //Ende: Von UTC nach MEZ



  //Start: Stundenzahl ausgeben

  if (mezaktstunde != oldstunde) {

    oldstunde = mezaktstunde;

    //Bereich für Stundenzahlen auf Null setzen
    for (i = 51; i <= 120; i++) {
      newletter[i] = 0;
    }

    startadresse = STUNDENADR[mezaktstunde][0];
    endadresse = STUNDENADR[mezaktstunde][1];

    for (i = startadresse; i <= endadresse; i++) {
      newletter[i] = 1;
    }
  }

  //Ende: Stundenzahl ausgeben



  //Start: Minutenzahl ausgeben

  if (mezaktminute != oldminute) {

    //Pointer zurücksetzen
    pointer = 99;

    //Minutenzahl für den nächsten Vergleich speichern
    oldminute = mezaktminute;

    //Bereich für Minutenzahlen auf Null setzen
    for (i = 8; i <= 48; i++) {
      newletter[i] = 0;
    }

    //Wort "gli" zurücksetzen
    newletter[8] = 0;
    newletter[9] = 0;
    newletter[10] = 0;

    //Wort "gsi" zurücksetzen
    newletter[117] = 0;
    newletter[118] = 0;
    newletter[119] = 0;

    //Füf
    if (mezaktminute >= 3 && mezaktminute <= 7 || mezaktminute >= 53 && mezaktminute <= 57) {
      pointer = 0;
    }

    //Zää
    if (mezaktminute >= 8 && mezaktminute <= 12 || mezaktminute >= 48 && mezaktminute <= 52) {
      pointer = 1;
    }

    //Viertu
    if (mezaktminute >= 13 && mezaktminute <= 17 || mezaktminute >= 43 && mezaktminute <= 47) {
      pointer = 2;
    }

    //Zwänzg
    if (mezaktminute >= 18 && mezaktminute <= 22 || mezaktminute >= 38 && mezaktminute <= 42) {
      pointer = 3;
    }

    //Haubi
    if (mezaktminute >= 28 && mezaktminute <= 32) {
      pointer = 4;
    }

    //Ausgabe der aktuellen Minutenzahl
    if (pointer >= 0 && pointer <= 4) {
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }
    }

    //Füf vor/ab Haubi
    if (mezaktminute >= 23 && mezaktminute <= 27 || mezaktminute >= 33 && mezaktminute <= 37) {

      //Füf
      pointer = 0;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }

      //Haubi
      pointer = 4;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }
    }

    //Wort "ab"
    if (mezaktminute >= 3 && mezaktminute <= 22 || mezaktminute >= 33 && mezaktminute <= 37) {
      pointer = 5;
    }

    //Wort "vor"
    if (mezaktminute >= 23 && mezaktminute <= 27 || mezaktminute >= 38 && mezaktminute <= 57) {
      pointer = 6;
    }

    //Wort "gli"
    if (mezaktminute >= 3 && mezaktminute <= 4 || mezaktminute >= 8 && mezaktminute <= 9 || mezaktminute >= 13 && mezaktminute <= 14 || mezaktminute >= 18 && mezaktminute <= 19 || mezaktminute >= 23 && mezaktminute <= 24 || mezaktminute >= 28 && mezaktminute <= 29 || mezaktminute >= 33 && mezaktminute <= 34 || mezaktminute >= 38 && mezaktminute <= 39 || mezaktminute >= 43 && mezaktminute <= 44 || mezaktminute >= 48 && mezaktminute <= 49 || mezaktminute >= 53 && mezaktminute <= 54 || mezaktminute >= 58 && mezaktminute <= 59) {
      //gli
      pointer = 7;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      //Wort "ab"
      if (mezaktminute >= 3 && mezaktminute <= 22 || mezaktminute >= 33 && mezaktminute <= 37) {
        pointer = 5;
      }

      //Wort "vor"
      if (mezaktminute >= 23 && mezaktminute <= 27 || mezaktminute >= 38 && mezaktminute <= 57) {
        pointer = 6;
      }

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }
    }

    //Wort "gsi"
    if (mezaktminute >= 1 && mezaktminute <= 2 || mezaktminute >= 6 && mezaktminute <= 7 || mezaktminute >= 11 && mezaktminute <= 12 || mezaktminute >= 16 && mezaktminute <= 17 || mezaktminute >= 21 && mezaktminute <= 22 || mezaktminute >= 26 && mezaktminute <= 27 || mezaktminute >= 31 && mezaktminute <= 32 || mezaktminute >= 36 && mezaktminute <= 37 || mezaktminute >= 41 && mezaktminute <= 42 || mezaktminute >= 46 && mezaktminute <= 47 || mezaktminute >= 51 && mezaktminute <= 52 || mezaktminute >= 56 && mezaktminute <= 57) {
      //gsi
      pointer = 8;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      //Wort "ab"
      if (mezaktminute >= 3 && mezaktminute <= 22 || mezaktminute >= 33 && mezaktminute <= 37) {
        pointer = 5;
      }

      //Wort "vor"
      if (mezaktminute >= 23 && mezaktminute <= 27 || mezaktminute >= 38 && mezaktminute <= 57) {
        pointer = 6;
      }

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }
    }

    //Ausgabe der aktuellen Minutenzahl
    if (pointer == 5 || pointer == 6) {
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];

      for (i = startadresse; i <= endadresse; i++) {
        newletter[i] = 1;
      }
    }

    //Ende: Minutenzahl ausgeben



    //Start: Ausgabe des Bildes

    //Kontrolle ob sich das Bild verändert hat
    for (i = ersteLED; i <= letzteLED; i++) {

      //Vergleich: altes Bild mit neuem Bild
      if (showletter[i] != newletter[i]) {

        newdisplay = HIGH;

        break;
      }
    }

    //wenn ja, dann Ausgabe des neuen Bildes
    //oder wenn neue Farbwerte über die App eingestellt wurden
    if (newdisplay == HIGH) {

      //neues Bild übergeben
      for (i = ersteLED; i <= letzteLED; i++) {

        showletter[i] = newletter[i];

        //LED einschalten
        if (showletter[i] == 1) {

          pixels.setPixelColor(i, pixels.Color(red, green, blue));
          pixels.show();
        }

        //LED ausschalten
        if (showletter[i] == 0) {

          pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          pixels.show();
        }
      }

      newdisplay = LOW;
    }

    //Ende: Ausgabe des Bildes



    //Start: Ausgabe fürs Debuging

    if (debug == HIGH) {

      if (millis() - lastmillisDebug > intervallDebug) {

        //Systemzeit speichern für den nächsten Vergleich
        lastmillisDebug += intervallDebug;

        Serial.print("NTP-Paket: ");
        Serial.println(newsecsSince1900);

        Serial.print("Uhrzeit UTC: ");
        Serial.print(utcaktstunde);
        Serial.print(":");
        Serial.print(utcaktminute);
        Serial.print(":");
        Serial.println(utcaktsekunde);

        Serial.print("Zeitverschiebung: ");
        Serial.println(utcx);

        Serial.print("Uhrzeit MEZ: ");
        Serial.print(mezaktstunde);
        Serial.print(":");
        Serial.print(mezaktminute);
        Serial.print(":");
        Serial.println(mezaktsekunde);

        Serial.print("Datum: ");
        Serial.print(datumTag);
        Serial.print(".");
        Serial.print(datumMonat);
        Serial.print(".");
        Serial.println(datumJahr);

        Serial.print("Wochentag: ");
        Serial.println(wochenTag);

        Serial.print("Schaltjahr: ");
        Serial.println(schaltJahr);

        Serial.print("RGB-Farbwert Rot: ");
        Serial.println(red);

        Serial.print("RGB-Farbwert Grün: ");
        Serial.println(green);

        Serial.print("RGB-Farbwert Blau: ");
        Serial.println(blue);

        //Lokale IP-Adresse ausgeben
        Serial.print("lokale IP-Adresse: ");
        Serial.println(WiFi.localIP());

        Serial.println();

      }
    }

    //Ende: Ausgabe fürs Debuging

  }
}

//Funktion zur Anfrage eines NTP-Paketes
void sendNTPpacket(IPAddress & address) {
  memset(paketpuffer, 0, ntppaket);

  //Erstes Byte
  //Leap Indicator: Bit 0, 1 => 11 => 3 => Schaltsekunde wird nicht synchronisiert
  //Version: Bit Bit 2, 3, 4 => 000 => 0 => Version xy?
  //Mode: Bit 5, 6, 7 => 111 => 7 => für den Privaten Gebrauch
  paketpuffer[0] = 0b11100011;

  //Zweites Byte
  //Stratum: Bit 0 - 7 => 00000000 => 0 => nicht angegeben
  paketpuffer[1] = 0;

  //Drittes Byte
  //Poll: Bit 0 - 7 => 00000110 => 6 => Abfrageintervall
  paketpuffer[2] = 6;

  //Viertes Byte
  //Precision: Bit 0 - 7 => 0xEC => Taktgenauigkeit
  paketpuffer[3] = 0xEC;

  //Die nächsten 8 Byte haben nur für Servernaachrichten eine Bedeutung
  //8 Bytes => 0

  paketpuffer[12]  = 49;
  paketpuffer[13]  = 0x4E;
  paketpuffer[14]  = 49;
  paketpuffer[15]  = 52;

  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(paketpuffer, ntppaket);
  udp.endPacket();
}



//Funktion zur Berechnung des Wochentages => Montag = 1; Dienstag = 2 usw.
unsigned int wochenTag_calc(unsigned long tage1900) {

  //Der 1. Januar 1900 war ein Montag
  unsigned int ergebnis = 1;

  int for_i = 0;

  for (for_i = 0; for_i < tage1900; for_i++) {

    if (ergebnis < 7) {
      ergebnis = ergebnis + 1;
    }

    else {
      ergebnis = 1;
    }
  }

  return ergebnis;
}



//Funktion zur Erkennung ob ein Schaltjahr vorliegt
bool schaltJahrJN(unsigned int jahr) {

  bool ergebnis_jn = LOW;

  //Erkennung von Schaltjahren
  if ((jahr % 4) == 0) {

    ergebnis_jn = HIGH;

    if ((jahr % 100) == 0) {

      ergebnis_jn = LOW;

      if ((jahr % 400) == 0) {

        ergebnis_jn = HIGH;
      }
    }
  }

  else {
    ergebnis_jn = LOW;
  }

  return ergebnis_jn;
}



//Funktion zur Berechnung des Monates
int welcherMonat(int tg_nr, bool schaltjn) {

  //Monatsanfänge
  int monatMin[13] = {0, 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
  //Monatsenden
  int monatMax[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

  int datum = 0;

  int y = 0;

  //Berechnung des Anfang und Ende jeden Montas im Schaltjahr
  if (schaltjn == HIGH) {

    for (y = 3; y < 13; y++) {
      monatMin[y] = monatMin[y] + 1;
    }

    for (y = 2; y < 13; y++) {
      monatMax[y] = monatMax[y] + 1;
    }
  }

  //Monat Januar
  if (tg_nr >= monatMin[1] && tg_nr <= monatMax[1]) {
    datum = 1;
  }

  //Monat Februar
  if (tg_nr >= monatMin[2] && tg_nr <= monatMax[2]) {
    datum = 2;
  }

  //Monat März
  if (tg_nr >= monatMin[3] && tg_nr <= monatMax[3]) {
    datum = 3;
  }

  //Monat April
  if (tg_nr >= monatMin[4] && tg_nr <= monatMax[4]) {
    datum = 4;
  }

  //Monat Mai
  if (tg_nr >= monatMin[5] && tg_nr <= monatMax[5]) {
    datum = 5;
  }

  //Monat Juni
  if (tg_nr >= monatMin[6] && tg_nr <= monatMax[6]) {
    datum = 6;
  }

  //Monat Juli
  if (tg_nr >= monatMin[7] && tg_nr <= monatMax[7]) {
    datum = 7;
  }

  //Monat August
  if (tg_nr >= monatMin[8] && tg_nr <= monatMax[8]) {
    datum = 8;
  }

  //Monat September
  if (tg_nr >= monatMin[9] && tg_nr <= monatMax[9]) {
    datum = 9;
  }

  //Monat Oktober
  if (tg_nr >= monatMin[10] && tg_nr <= monatMax[10]) {
    datum = 10;
  }

  //Monat November
  if (tg_nr >= monatMin[11] && tg_nr <= monatMax[11]) {
    datum = 11;
  }

  //Monat Dezember
  if (tg_nr >= monatMin[12] && tg_nr <= monatMax[12]) {
    datum = 12;
  }

  return datum;
}
