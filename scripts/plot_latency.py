import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sklearn.preprocessing import MinMaxScaler

try:
    df_mutex = pd.read_csv("../csvs/mutex_ns.csv", header=None, names=['lat'])
    df_sem = pd.read_csv("../csvs/sem_ns.csv", header=None, names=['lat'])
except FileNotFoundError as e:
    print(f"Error: No se encontró el archivo {e.filename}.")
    exit()

scaler = MinMaxScaler()
combined = pd.concat([df_mutex, df_sem])
scaler.fit(combined)

scaled_mutex = scaler.transform(df_mutex)
scaled_sem = scaler.transform(df_sem)

plt.figure(figsize=(12, 7))
min_limit = min(df_mutex['lat'].min(), df_sem['lat'].min())
max_limit = max(df_mutex['lat'].max(), df_sem['lat'].max())
bins = np.linspace(min_limit, max_limit, 100)
plt.hist(df_mutex['lat'], bins=bins, alpha=0.7, label='Mutex', color='skyblue')
plt.hist(df_sem['lat'], bins=bins, alpha=0.7, label='Semáforo', color='salmon')
plt.title('Distribución de Latencias en Idle (Mutex vs Semáforo)', fontsize=16)
plt.xlabel('Latencia (ns)', fontsize=12)
plt.ylabel('Frecuencia', fontsize=12)
plt.legend()
plt.grid(True, linestyle='--', alpha=0.6)
plt.show()
