
//--------------------------------------------------------
// Arduino sensor reader
//--------------------------------------------------------

const int start_pin = 7;
const int sin_pin = 6;
const int load_pin = 5;
const int reset_pin = 4;
const int xck_pin = 3;
const int read_pin = 2;

//--------------------------------------------------------

void setup()
{
  pinMode(start_pin, INPUT);
  pinMode(sin_pin, INPUT);
  pinMode(load_pin, INPUT);
  pinMode(reset_pin, INPUT);
  pinMode(xck_pin, INPUT);
  pinMode(read_pin, INPUT);

  Serial.begin(57600);
}

//--------------------------------------------------------

unsigned int clocks_elapsed = 0;

int last_xck = 0;
int last_read = 0;

unsigned int register_write_data_address = 0;

void loop()
{
  int cur_xck = digitalRead(xck_pin);
  if(cur_xck != last_xck)
  {
    last_xck = cur_xck;
    if(cur_xck == HIGH)
    {
      //Check START, SIN, RESET, READ
      register_write_data_address  = (register_write_data_address << 1) | (digitalRead(sin_pin) == HIGH); // SIN
      
      if(digitalRead(reset_pin) == LOW) // RESET
      {
        char str[50]; sprintf(str,"RESET %u",clocks_elapsed); clocks_elapsed = 0;
        Serial.println(str);
      }
      if(digitalRead(start_pin) == HIGH) // START
      {
        char str[50]; sprintf(str,"START %u",clocks_elapsed); clocks_elapsed = 0;
        Serial.println(str);
      }
      
      int cur_read = digitalRead(read_pin); // READ
      if(last_read != cur_read)
      {
        last_read = cur_read;
        char str[50]; sprintf(str,cur_read ? "READ START %u" : "READ END %u",clocks_elapsed); clocks_elapsed = 0;
        Serial.println(str);
      }
    }
    else
    {
      //Check LOAD
      if(digitalRead(load_pin))
      {
        unsigned int addr = (register_write_data_address>>8)&7;
        unsigned int value = register_write_data_address&0xFF;
        char str[50]; sprintf(str,"LOAD REG %u ([%X] = %X)",clocks_elapsed,addr,value); clocks_elapsed = 0;
        Serial.println(str);
        register_write_data_address = 0;
      }
      clocks_elapsed ++;
    }
  }
}

//--------------------------------------------------------

