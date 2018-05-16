import matplotlib.pyplot as plt
import numpy as np

time = []
target = 0
side = []
top = []
brewhead = []
heater = []
pump = []
pid_u = []
pid_p = []
pid_i = []
pid_d = []


# with open("data_p40.0_i2.0_d0.0.csv", "r") as f:
# with open("data_p20.0_i0.6_d15.0.csv", "r") as f:
# with open("data_p20.0_i0.5_d15.0.csv", "r") as f:
with open("shot_p25.0_i0.5_d15.0.csv", "r") as f:

    for line in f:
        myline = f.readline()
        # print(myline)
        if myline != "" and myline[0].isdigit():
            time_, target_, side_, top_, brewhead_, heater_, pump_, u_, p_, i_, d_ = myline.split(' , ')
            time.append(int(time_))
            target = float(target_)
            side.append(float(side_))
            top.append(float(top_))
            heater.append(int(heater_))
            pump.append(int(pump_))
            pid_u.append(float(u_))
            pid_p.append(float(p_))
            pid_i.append(float(i_))
            pid_d.append(float(d_))

fig, (ax1a, ax1b) = plt.subplots(2, 1)
plt.title("PID - todo - parameters")
ax1b.set_xlabel('time [s]')
ax1a.set_ylabel('temp [C]')
ax2a = ax1a.twinx()
ax2a.set_ylabel('heater percent')

ax2a.yaxis.set_ticks(np.arange(0, 100 + 1, 10))

time_step = (time[1] - time[0])/1000.0
x_time = np.arange(0, len(time)*time_step, time_step)

ax2a.plot(x_time, heater, 'b', label='Heater Percent')
ax2a.plot(x_time, pump, 'y', label='Pump Percent')
ax2a.plot(x_time, pid_u, label='u')

ax1a.axhline(target, color='g')

ax1a.plot(x_time, top, 'k', label='top')
ax1a.plot(x_time, side, 'r', label='side')

ax1b.plot(x_time, pid_p, label='P')
ax1b.plot(x_time, pid_i, label='I')
ax1b.plot(x_time, pid_d, label='D')


ax1a.legend()
ax1b.legend()
ax2a.legend()
# plt.legend()
ax1a.grid(True)
ax1b.grid(True)
fig.tight_layout()
plt.show()