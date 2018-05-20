from builtins import print

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
# with open("shot_p25.0_i0.5_d15.0.csv", "r") as f:
# with open("data_p80.0_i4.0_d15.0.csv", "r") as f:
# with open("data_p80.0_i6.0_d15.0.csv", "r") as f:
# with open("data_p100.0_i4.0_d15.0.csv", "r") as f:
# with open("data_p100.0_i3.0_d15.0.csv", "r") as f:
# with open("data_p150.0_i3.0_d50.0.csv", "r") as f:
# with open("data_p150.0_i4.0_d20.0.csv", "r") as f:
# with open("data_p100.0_i4.0_d20.0.csv", "r") as f:
# with open("data_p70.0_i5.0_d20.0.csv", "r") as f:
# with open("const_5perc.txt", "r") as f:
# with open("const_1perc", "r") as f:  # max 93
# with open("cold_boot_100perc_heater", "r") as f:  # max 95
# with open("data_p11.0_i38.0_d9.5_test.csv", "r") as f:
# with open("data_p11.0_i3.0_d9.5.csv", "r") as f:
# with open("data_p11.0_i3.0_d9.5_sens_avg.csv", "r") as f:
# with open("data_p100.0_i1.0_d0.0.csv", "r") as f:
# with open("data_p50.0_i1.0_d0.0.csv", "r") as f:
# with open("data_p50.0_i1.5_d0.0.csv", "r") as f:
# with open("data_p25.0_i1.0_d0.0.csv", "r") as f:
# with open("data_p25.0_i0.6_d0.0.csv", "r") as f:
# with open("data_p25.0_i0.4_d10.0.csv", "r") as f:
# with open("data_p25.0_i0.4_d10.0_wasserspiele.csv", "r") as f:
# with open("data_p25.0_i0.4_d10.0_limited_5.csv", "r") as f:
with open("data_p25.0_i0.4_d10.0_limited_5_shot.csv", "r") as f:

    for line in f:
        myline = f.readline()
        # print(myline)
        if myline != "" and myline[0].isdigit():
            time_, target_, side_, top_, brewhead_, heater_, pump_, u_, p_, i_, d_ = myline.split(' , ')
            time.append(int(time_))
            target = float(target_)
            side.append(float(side_))
            top.append(float(top_))
            brewhead.append(float(brewhead_))
            heater.append(int(heater_))
            pump.append(int(pump_))
            pid_u.append(float(u_))
            pid_p.append(float(p_))
            pid_i.append(float(i_))
            pid_d.append(float(d_))
        else:
            print(myline)

fig, (ax1a, ax1b) = plt.subplots(2, 1, sharex=True)
plt.axes(ax1a)
plt.title("PID - todo - parameters")
ax1b.set_xlabel('time [s]')
ax1a.set_ylabel('temp [C]')
ax2a = ax1a.twinx()
ax2a.set_ylabel('heater percent')

ax2a.set_ylim([-10, 110])
ax2a.yaxis.set_ticks(np.arange(0, 100 + 1, 10))

time_step = (time[1] - time[0])/1000.0
x_time = np.arange(0, len(time)*time_step, time_step)

ax2a.plot(x_time, heater, 'b', label='Heater Percent')
ax2a.plot(x_time, pump, 'y', label='Pump Percent')
ax2a.plot(x_time, pid_u, label='u')

if 0:  # step-analysis
    # ax1a.set_ylim([-10, 100])
    # ax1a.yaxis.set_ticks(np.arange(-10, 100 + 1, 10))
    used_sensor = side
    heater_percent_step  = np.max(heater) - np.min(heater)
    print("Heater step min:", np.min(heater), "max:", np.max(heater))

    idx_heater_start = np.argmax(heater)
    ax1a.axvline(x_time[idx_heater_start], color='g')

    temp_heater_start = used_sensor[idx_heater_start]
    time_heater_start = x_time[idx_heater_start]
    temp_heater_stop = 95
    delta_temp = temp_heater_stop - temp_heater_start
    print("START: Temp at time", time_heater_start, "s:", temp_heater_start, "degC")
    time_10perc_heatup = x_time[np.argmax(np.array(used_sensor[idx_heater_start:]) > temp_heater_start + delta_temp*0.1) + idx_heater_start]
    time_90perc_heatup = x_time[np.argmax(np.array(used_sensor[idx_heater_start:]) > temp_heater_stop - delta_temp*0.1) + idx_heater_start]
    print("time start-temp + 10%:", time_10perc_heatup, "from t0:", time_10perc_heatup - time_heater_start)
    print("time stop-temp - 10%:", time_90perc_heatup, "from t0:", time_90perc_heatup - time_heater_start)
    print("delta-t s:", time_90perc_heatup - time_10perc_heatup)
    temp_10perc_heatup = temp_heater_start + delta_temp*0.1
    temp_90perc_heatup = temp_heater_stop - delta_temp*0.1
    print("start-temp + 10%:", temp_10perc_heatup)
    print("stop-temp - 10%:", temp_90perc_heatup)
    print("delta-T 80% K:", temp_90perc_heatup - temp_10perc_heatup)
    print("delta-T 100% K:", temp_heater_stop - temp_heater_start)
    temp_tau_third = temp_heater_start + 0.283 * (temp_heater_stop - temp_heater_start)
    temp_tau = temp_heater_start + 0.632 * (temp_heater_stop - temp_heater_start)
    idx_tau_third = np.argmax(np.array(used_sensor[idx_heater_start:]) > temp_tau_third) + idx_heater_start
    idx_tau = np.argmax(np.array(used_sensor[idx_heater_start:]) > temp_tau) + idx_heater_start
    time_tau_third = x_time[idx_tau_third]
    time_tau = x_time[idx_tau]
    print("tau/3: temp:", temp_tau_third, "K, time after t0:", time_tau_third - time_heater_start)
    print("tau: temp:", temp_tau, "K, time after t0:", time_tau - time_heater_start)
    # slope
    slope_deg_per_s = (temp_tau - temp_tau_third) / ((time_tau - time_heater_start) - (time_tau_third - time_heater_start))
    # heater_percent_step % difference heater
    slope = slope_deg_per_s / heater_percent_step
    print("slope K / (%*s):", slope)
    ax1a.axvline(x_time[idx_tau_third], color='g')
    ax1a.axvline(x_time[idx_tau], color='g')
    ax1a.plot(x_time, x_time*slope_deg_per_s + used_sensor[idx_tau_third] - (x_time*slope_deg_per_s)[idx_tau_third], 'yellow', label='slope')
else:
    ax1a.axhline(target, color='g')

ax1a.plot(x_time, top, 'k', label='top')
ax1a.plot(x_time, side, 'r', label='side')
ax1a.plot(x_time, np.add(top, side)/2, 'pink', label='avg')
ax1a.plot(x_time, brewhead, 'grey', label='brewhead')

ax1b.plot(x_time, pid_p, label='P')
ax1b.plot(x_time, pid_i, label='I')
ax1b.plot(x_time, pid_d, label='D')
combined = np.add(pid_p, pid_i)
combined = np.add(combined, pid_d)
ax1b.plot(x_time, combined, label='PID')


ax1a.legend(loc="upper right")
ax1b.legend(loc="best")
ax2a.legend(loc="upper left")
# plt.legend()
ax1a.grid(True)
ax1b.grid(True)
fig.tight_layout()
plt.show()