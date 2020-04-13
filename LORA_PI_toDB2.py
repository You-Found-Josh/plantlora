import time
from time import sleep
from SX127x.LoRa import *
from SX127x.board_config import BOARD
from datetime import datetime
from influxdb import InfluxDBClient
import struct #delete if not using Struct

# Configure InfluxDB connection variables
host = 'localhost' # My Ubuntu NUC
port = 8086 # default port
user = 'admin' # the user/password created for the pi, with write access
password = 'Ju50stIz'
dbname = 'sensor_data' # the database we created earlier
interval = 60 # Sample period in seconds

dataPayload = 0
payload = 0


# Create the InfluxDB client object
client = InfluxDBClient(host, port, user, password, dbname)

BOARD.setup()

class LoRaRcvCont(LoRa):
    def __init__(self, verbose=False):
        super(LoRaRcvCont, self).__init__(verbose)
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)

    def start(self):
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)
        while True:
            sleep(.5)
            rssi_value = self.get_rssi_value()
            status = self.get_modem_status()
            sys.stdout.flush()
            

    def on_rx_done(self):
        # current date and time
        now = datetime.now()
        format = "%a %b %d %H:%M:%S %Y"
        

        self.clear_irq_flags(RxDone=1)
        payload = self.read_payload(nocheck=True)
        dateTimeObj = datetime.now()
        shortTime = now.strftime(format)
        dataPayload = bytes(payload).decode("utf-8", errors='ignore')
        # dataPayload = payload.decode(encoding='ascii',errors='ignore')

        # print(payload)

        print(shortTime,"- Full Payload: ",dataPayload)

        data = dataPayload.split("|") #splitting the payload into a list of strings
        
        SoilMoisture = data[0]
        # SoilMoisture = int(data[0]) #ValueError: invalid literal for int() with base 10: '\x00\x0027'
        # SoilMoistureTest = data[0].decode('utf-8', errors='ignore') #AttributeError: 'str' object has no attribute 'decode'
        # SoilMoistureTest = struct.unpack('<H', data[0]) #TypeError: a bytes-like object is required, not 'str'
        # SoilMoistureTest = int(data[0],2) #ValueError: invalid literal for int() with base 2: '\x00\x0032'
        Celcius_Int = int(data[1])
        Celcius = data[1]
        Fahrenheit = 9.0/5.0 * Celcius_Int + 32
        AirPressure = data[2]
        Altitude = data[3]
        Humidity = data[4]
        Light = data[5]

        print(shortTime,"- Soil Moisture Percentage: ",SoilMoisture,"%")
        # print(shortTime,"- Soil Moisture Test: ",SoilMoistureTest,"%")
        print(shortTime,"- Air Temperature: ",Celcius,"C")  
        print(shortTime,"- Air Temperature: ",Fahrenheit,"F")
        print(shortTime,"- Air Pressure: ",AirPressure,"hPa")
        print(shortTime,"- Altitude: ",Altitude,"m")  
        print(shortTime,"- Air Humidity Percentage: ",Humidity,"%") 
        print(shortTime,"- Light Percent: ",Light,"%")  
        print('\n')       

        self.set_mode(MODE.SLEEP)
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT) 

        # print("[%s] Temp: %s, Humidity: %s" % (iso, temperature, humidity)) 
        # Create the JSON data structure

        # influx_data = [
        # {
        #   "measurement": "sensor_data",
        #       "tags": {
        #           "location": "Dining Room",
        #       },
        #       "time": time.ctime(),
        #       "fields": {
        #           "soil_moisture" : data[0],
        #           "temperature_c": Celcius,
        #           "temperature_f": Fahrenheit,
        #           "pressure": data[2],
        #           "altitude": data[3],
        #           "humidity": data[4],
        #           "light": data[5]
        #       }
        #   }
        # ]

        # if client.write_points(influx_data) == True:
        #     print("Data written to DB successfully")
        # else:  # write failed.
        #     print("DB write failed.")

            # time.sleep(interval)


lora = LoRaRcvCont(verbose=False)
lora.set_mode(MODE.STDBY)

#  Medium Range  Defaults after init are 434.0MHz, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on 13 dBm

lora.set_pa_config(pa_select=1)

try:
    lora.start()
except KeyboardInterrupt:
    sys.stdout.flush()
    print("")
    sys.stderr.write("KeyboardInterrupt\n")
finally:
    sys.stdout.flush()
    print("")
    lora.set_mode(MODE.SLEEP)
    BOARD.teardown()
