
//--------------------------------------------------------
// Arduino sensor reader
//--------------------------------------------------------

const int start_pin = 7;
const int sin_pin = 6;
const int load_pin = 5;
const int reset_pin = 4;
const int xck_pin = 3;
const int read_pin = 2;

#define BIT(n) (1<<(n))

//--------------------------------------------------------

void setup()
{
  pinMode(start_pin, INPUT);
  pinMode(sin_pin, INPUT);
  pinMode(load_pin, INPUT);
  pinMode(reset_pin, INPUT);
  pinMode(xck_pin, INPUT);
  pinMode(read_pin, INPUT);

  Serial.begin(19200);
}

//--------------------------------------------------------


void loop()
{

}

//--------------------------------------------------------

