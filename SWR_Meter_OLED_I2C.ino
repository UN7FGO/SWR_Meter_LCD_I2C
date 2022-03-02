// http://un7fgo.gengen.ru (C) 2019
// 
// Скетч КСВ-метра с LCD экраном 2х16 символов, подключенным по протоколу I2c
// 
// При подаче на цифровой вход MODE_PIN низкого уровня сигнала, устройство переходит в режим калибровки
// В этом режиме на экране отображаются данные считываемые непосредственно с аналоговых входов Arduino.
// Исходя из фактически получаемых данных мы можем корректно вычислить коэфициенты для расчета мощности.
// Мощность расчитывается квадратичной функцией Y = A*x*x + B*x + C,
// поскольку на входе мы измеряем напряжение, а мошность пропорциональна квадрату напряжения - P = U*U/R .
// Остальные коэфициенты служат для корректировки нелинейности наших цепей измерения.
// Подробнее о методике расчета коэфициентов и зачем оно все нужно, читаем тут - http://blog.gengen.ru/?p=1412
// ---------------------------------------------
// Подключаем библиотеки 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
  
// ---------------------------------------------
// глобальные константы

// контакт для управления режимом работы устройства
#define MODE_PIN 2

// контакты для считывания показаний прямой и отраженной мощности
#define POW_PIN A0
#define REF_PIN A1

// параметры усреднения считываемых показаний
// количество считываний, для расчета среднего
#define AVG_COUNT 5
// задержка между считываниями показаний датчика, для усреднения, в миллисекундах
#define AVG_TIME 20

// Вариант расчета КСВ
// true - "правильный" - по нормализованным значениям напряжения, 
//        исходя из откалиброванных измерений мощности
// false - "тупой" - просто по входным данным с входов микроконтроллера
#define SWR_MATH true


// ---------------------------------------------
// глобальные переменные для работы программы

// переменные для считывания и усреднения показаний с датчика
  int PW;
  int RF;

// Предполагаем прараболическую зависимость подводимой мошности от измеряемого нами напряжения
// Расчетные коэфициенты для уравнений типа P = A*X*X + B*X + C (желательно с точностью не менее 6 знаков после запятой)
// Коэфициенты уравнения для Прямой мощности
  float PA = 0.0000000;
  float PB = 1.0000000;
  float PC = 0.0000000;
// Коэфициенты уравнения для Обратной мощности
  float RA = 0.0000000;
  float RB = 1.0000000;
  float RC = 0.0000000;

// Переменные для расчета мощностей и КСВ
  float POW;
  float REF;
  float SWR;

// временные переменные
  String S1;
  String S2;
  int bl;
  float PWr;
  float RFr;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------------------------------------
// Процедура инициализации
void setup()
{

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setCursor(0, 0);     // Start at top-left corner
  display.setTextColor(SSD1306_WHITE);
  display.print(F("SWR METER"));
  display.setCursor(0, 32);     // Start at top-left corner
  display.print(F("VER. 0.99"));
  display.display();
  delay(2000); 
  
// определяем режим работы аналоговых выводов  
  pinMode(MODE_PIN,INPUT);
  pinMode(POW_PIN,INPUT);
  pinMode(REF_PIN,INPUT);
  
// прочие переменные 
  bl = 1;
  SWR = 1.05;
}

// ---------------------------------------------
// Основное тело программы
void loop()  // цикл
{
// считываем данные с аналоговых входов и усредняем
  PW = 0;
  RF = 0;
  for (int i=1; i <= AVG_COUNT; i++) {
    PW = PW + analogRead(POW_PIN);
    RF = RF + analogRead(REF_PIN);
    delay(AVG_TIME);
  }
  PW = PW / AVG_COUNT;
  RF = RF / AVG_COUNT;    

  if (digitalRead(MODE_PIN) == LOW)  {
// Работа в режиме "клибровки".
// В этом режиме на экран выводятся "сырые" данные со входов микроконтроллера.
  display.clearDisplay();
  display.setCursor(0, 0);     // Start at top-left corner
  display.print(F("CALIBR."));
  display.setCursor(0, 24);     // Start at top-left corner
  display.print(F("FW"));
  display.setCursor(32, 24);     // Start at top-left corner
  display.print(PW);
  display.setCursor(0, 44);     // Start at top-left corner
  display.print(F("RF"));
  display.setCursor(32, 44);     // Start at top-left corner
  display.print(RF);
  display.display();
  } else {
// Режим вывода на экран результатов расчета реальной мощности и КСВ

    // пересчитываем считанные показания в реальную мощность
    POW = PA*PW*PW + PB*PW + PC;
    if (POW < 0) {
      POW = 0;
    }
    REF = RA*RF*RF + RB*RF + RC;
  
    // вычисляем КСВ
    if (SWR_MATH) {
      // Поскольку входное напряжение у нас не нормировано 
      // т.е. никто не настраивал с высокой точностью его соответствие между каналами
      // а для расчета КСВ используется напряжение, то мы его получим расчетным путем
      // исходя из того, что при расчете измеряемой мощности у нас учтены все нелинейности каждого канала измерения
      PWr = sqrt(POW * 50);
      RFr = sqrt(REF * 50);
    } else {
      // Считаем тупо исходя из показаний аналоговых входов микроконтрллера 
      PWr = float(PW);
      RFr = float(RF);
    }
    SWR = (PWr + RFr)/(PWr - RFr);       
    
    // чтобы не пугать нас при отключенном передатчике, КСВ присваивем 1.0
    if ( POW <=0 ) {
      SWR = 1.00;
    }
    // чтобы не пугать нас при "непонятном" КСВ менее 1, КСВ присваивем 0.99
    // это может получиться при перепутывании контактов датчика
    if (SWR < 1 && POW >0 ) {
      SWR = 0.99;
    }
    // чтобы не "пугаться" большим цифрам, все что больще 30, будет показываться как 29.9
    if (SWR > 30) {
      SWR = 29.9;
    }

    // Выводим обработанные данные на дисплей
    display.clearDisplay();
    display.setCursor(0, 0);     // Start at top-left corner
    display.print(F("FW"));
    display.setCursor(32, 0);     // Start at top-left corner
    display.print(FormatI(POW,4));
    display.setCursor(0, 20);     // Start at top-left corner
    display.print(F("RF"));
    display.setCursor(32, 20);     // Start at top-left corner
    display.print(FormatI(REF,4));
    display.setCursor(0, 40);     // Start at top-left corner
    display.print(F("SWR"));
    display.setCursor(48, 40);     // Start at top-left corner
    display.print(SWR);
    display.display();
    delay(500);
  }
  // задержка перед обновлением экрана
  delay(500);
}

// изобретаю "велосипед", делаем число целым, строкой и добавляем незначащие пробелы перед числом
String FormatI(float A, byte B) {
  String S;
  S = String(int(A));
  while ( S.length()<B ) {
    S = " " + S;
  }
  return S;
}
