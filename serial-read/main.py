import serial
import csv
import sys

PORT   = "/dev/ttyUSB0"
BAUD   = 115200
OUTPUT = "dados_mpu6050.csv"

HEADER = ["timestamp_us","ax_g","ay_g","az_g","gx_dps","gy_dps","gz_dps","temp_c","dropped"]

def main():
    with serial.Serial(PORT, BAUD, timeout=2) as ser, \
         open(OUTPUT, "w", newline="") as f:

        writer = csv.writer(f)
        writer.writerow(HEADER)   
        f.flush()

        print(f"Lendo de {PORT}... Ctrl+C para parar.")
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            if line.startswith(("I ", "W ", "E ", "D ")):
                print(f"[LOG] {line}")
                continue

            # Ignora cabeçalho duplicado vindo do ESP32
            if line.startswith("timestamp_us"):
                continue

            fields = line.split(",")
            if len(fields) != 9:
                continue

            writer.writerow(fields)
            f.flush()

            ts, ax, ay, az, gx, gy, gz, temp, drop = fields
            print(f"[{int(ts)/1e6:10.3f}s] "
                  f"A: {float(ax):+.3f} {float(ay):+.3f} {float(az):+.3f} g | "
                  f"G: {float(gx):+.3f} {float(gy):+.3f} {float(gz):+.3f} °/s | "
                  f"{float(temp):.1f}°C drop={drop}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nEncerrado.")

