--
-- Copyright 2024 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- Device specific device curves with 1D dependency (i.e. curve characteristics
-- are dependent only on one CPU policy). See go/wattson for more info.
CREATE PERFETTO TABLE _device_curves_1d
AS
WITH data(device, policy, freq_khz, static, active, idle0, idle1) AS (
  VALUES
  -- 255 represents static curve; needs to be same type as rest
  ("monaco", 0, 614400, 4.8, 9.41, 0.76, 0),
  ("monaco", 0, 864000, 6.68, 13.64, 0.83, 0),
  ("monaco", 0, 1363200, 12.6, 24.83, 1.1, 0),
  ("monaco", 0, 1708800, 18.39, 39.69, 1.34, 0),
  ("Tensor", 4, 400000, 0, 28.51, 5.24, 0),
  ("Tensor", 4, 553000, 0, 43.63, 6.1, 0),
  ("Tensor", 4, 696000, 0, 54.73, 6.76, 0),
  ("Tensor", 4, 799000, 0, 65.01, 6.89, 0),
  ("Tensor", 4, 910000, 0, 80.33, 7.61, 0),
  ("Tensor", 4, 1024000, 0, 92.91, 8.32, 0),
  ("Tensor", 4, 1197000, 0, 132.46, 8.09, 0),
  ("Tensor", 4, 1328000, 0, 146.82, 9.6, 0),
  ("Tensor", 4, 1491000, 0, 183.2, 11.58, 0),
  ("Tensor", 4, 1663000, 0, 238.55, 12.02, 0),
  ("Tensor", 4, 1836000, 0, 259.04, 16.11, 0),
  ("Tensor", 4, 1999000, 0, 361.98, 15.7, 0),
  ("Tensor", 4, 2130000, 0, 428.51, 18.94, 0),
  ("Tensor", 4, 2253000, 0, 527.05, 23.5, 0),
  ("Tensor", 6, 500000, 0, 87.31, 16.14, 0),
  ("Tensor", 6, 851000, 0, 170.29, 19.88, 0),
  ("Tensor", 6, 984000, 0, 207.43, 20.55, 0),
  ("Tensor", 6, 1106000, 0, 251.88, 23.06, 0),
  ("Tensor", 6, 1277000, 0, 306.57, 25.12, 0),
  ("Tensor", 6, 1426000, 0, 382.61, 26.76, 0),
  ("Tensor", 6, 1582000, 0, 465.9, 29.74, 0),
  ("Tensor", 6, 1745000, 0, 556.25, 32.87, 0),
  ("Tensor", 6, 1826000, 0, 613.51, 36.01, 0),
  ("Tensor", 6, 2048000, 0, 758.89, 41.67, 0),
  ("Tensor", 6, 2188000, 0, 874.03, 47.92, 0),
  ("Tensor", 6, 2252000, 0, 949.55, 51.07, 0),
  ("Tensor", 6, 2401000, 0, 1099.53, 57.42, 0),
  ("Tensor", 6, 2507000, 0, 1267.19, 66.14, 0),
  ("Tensor", 6, 2630000, 0, 1500.6, 82.36, 0),
  ("Tensor", 6, 2704000, 0, 1660.81, 95.11, 0),
  ("Tensor", 6, 2802000, 0, 1942.89, 121.43, 0)
)
select * from data;

-- Device specific device curves with 2D dependency (i.e. curve characteristics
-- are dependent on another CPU policy). See go/wattson for more info.
CREATE PERFETTO TABLE _device_curves_2d
AS
WITH data(device, freq_khz, other_policy, other_freq_khz, static, active, idle0, idle1)
AS (
  VALUES
  -- 255 represents static curve; needs to be same type as rest
  ("Tensor", 300000, 4, 400000, 3.73, 21.84, 0.47, 0),
  ("Tensor", 300000, 4, 553000, 5.66, 18.97, 0.99, 0),
  ("Tensor", 300000, 6, 500000, 2.61, 22.89, 0.76, 0),
  ("Tensor", 574000, 4, 400000, 5.73, 35.85, 0.93, 0),
  ("Tensor", 574000, 4, 553000, 5.41, 36.54, 0.98, 0),
  ("Tensor", 574000, 4, 696000, 5.61, 32.98, 0.99, 0),
  ("Tensor", 574000, 4, 799000, 9.7, 40.29, 1.33, 0),
  ("Tensor", 574000, 4, 910000, 9.81, 44.42, 1.24, 0),
  ("Tensor", 574000, 4, 1024000, 9.71, 43.95, 1.31, 0),
  ("Tensor", 574000, 6, 500000, 5.6, 34.69, 1.03, 0),
  ("Tensor", 574000, 6, 851000, 5.57, 33.66, 1.02, 0),
  ("Tensor", 574000, 6, 984000, 5.68, 36.2, 0.98, 0),
  ("Tensor", 574000, 6, 1106000, 5.59, 36.27, 1.02, 0),
  ("Tensor", 738000, 4, 400000, 6.62, 47.66, 1.08, 0),
  ("Tensor", 738000, 4, 553000, 6.7, 45.71, 1.03, 0),
  ("Tensor", 738000, 4, 696000, 6.7, 46.21, 1.04, 0),
  ("Tensor", 738000, 4, 799000, 9.8, 55.47, 1.23, 0),
  ("Tensor", 738000, 4, 910000, 9.69, 52.58, 1.31, 0),
  ("Tensor", 738000, 4, 1024000, 9.77, 54.81, 1.3, 0),
  ("Tensor", 738000, 4, 1197000, 18.75, 75.3, 2.05, 0),
  ("Tensor", 738000, 4, 1328000, 18.98, 75.84, 1.91, 0),
  ("Tensor", 738000, 6, 500000, 6.63, 44.56, 1.11, 0),
  ("Tensor", 738000, 6, 851000, 6.65, 46.62, 1.08, 0),
  ("Tensor", 738000, 6, 984000, 6.63, 50.28, 1.08, 0),
  ("Tensor", 738000, 6, 1106000, 6.74, 44.83, 1.07, 0),
  ("Tensor", 738000, 6, 1277000, 6.6, 44.15, 1.09, 0),
  ("Tensor", 738000, 6, 1426000, 18.97, 74.73, 1.94, 0),
  ("Tensor", 930000, 4, 400000, 9.64, 81.16, 1.27, 0),
  ("Tensor", 930000, 4, 553000, 9.88, 67.4, 1.28, 0),
  ("Tensor", 930000, 4, 696000, 9.69, 67.33, 1.3, 0),
  ("Tensor", 930000, 4, 799000, 9.69, 67.82, 1.3, 0),
  ("Tensor", 930000, 4, 910000, 9.79, 67.52, 1.29, 0),
  ("Tensor", 930000, 4, 1024000, 9.75, 65.44, 1.28, 0),
  ("Tensor", 930000, 4, 1197000, 18.84, 83.73, 2.0, 0),
  ("Tensor", 930000, 4, 1328000, 18.88, 101.57, 1.97, 0),
  ("Tensor", 930000, 4, 1491000, 18.86, 94.45, 1.99, 0),
  ("Tensor", 930000, 4, 1663000, 35.46, 134.93, 3.29, 0),
  ("Tensor", 930000, 4, 1836000, 35.34, 135.55, 3.36, 0),
  ("Tensor", 930000, 6, 500000, 9.76, 66.0, 1.28, 0),
  ("Tensor", 930000, 6, 851000, 9.8, 73.08, 1.24, 0),
  ("Tensor", 930000, 6, 984000, 9.75, 74.87, 1.25, 0),
  ("Tensor", 930000, 6, 1106000, 9.68, 77.31, 1.3, 0),
  ("Tensor", 930000, 6, 1277000, 9.83, 80.03, 1.25, 0),
  ("Tensor", 930000, 6, 1426000, 19.01, 98.31, 1.94, 0),
  ("Tensor", 930000, 6, 1582000, 18.94, 94.51, 1.98, 0),
  ("Tensor", 930000, 6, 1745000, 19.0, 94.38, 1.93, 0),
  ("Tensor", 930000, 6, 1826000, 18.98, 100.84, 1.92, 0),
  ("Tensor", 1098000, 4, 400000, 12.93, 109.45, 1.47, 0),
  ("Tensor", 1098000, 4, 553000, 12.92, 120.82, 1.48, 0),
  ("Tensor", 1098000, 4, 696000, 13.09, 107.17, 1.41, 0),
  ("Tensor", 1098000, 4, 799000, 12.82, 91.84, 1.56, 0),
  ("Tensor", 1098000, 4, 910000, 12.88, 99.1, 1.52, 0),
  ("Tensor", 1098000, 4, 1024000, 12.81, 87.32, 1.57, 0),
  ("Tensor", 1098000, 4, 1197000, 18.92, 115.83, 1.97, 0),
  ("Tensor", 1098000, 4, 1328000, 18.97, 137.08, 1.93, 0),
  ("Tensor", 1098000, 4, 1491000, 18.94, 120.36, 1.94, 0),
  ("Tensor", 1098000, 4, 1663000, 35.21, 156.0, 3.43, 0),
  ("Tensor", 1098000, 4, 1836000, 35.21, 155.3, 3.42, 0),
  ("Tensor", 1098000, 4, 1999000, 35.49, 157.04, 3.24, 0),
  ("Tensor", 1098000, 4, 2130000, 35.17, 156.91, 3.41, 0),
  ("Tensor", 1098000, 6, 500000, 13.0, 93.54, 1.45, 0),
  ("Tensor", 1098000, 6, 851000, 13.12, 104.28, 1.4, 0),
  ("Tensor", 1098000, 6, 984000, 12.85, 94.73, 1.52, 0),
  ("Tensor", 1098000, 6, 1106000, 12.68, 95.73, 1.6, 0),
  ("Tensor", 1098000, 6, 1277000, 12.94, 92.78, 1.46, 0),
  ("Tensor", 1098000, 6, 1426000, 18.81, 128.5, 2.03, 0),
  ("Tensor", 1098000, 6, 1582000, 19.0, 124.51, 1.89, 0),
  ("Tensor", 1098000, 6, 1745000, 18.75, 121.84, 2.0, 0),
  ("Tensor", 1098000, 6, 1826000, 19.01, 117.69, 1.9, 0),
  ("Tensor", 1098000, 6, 2048000, 18.97, 107.49, 1.89, 0),
  ("Tensor", 1098000, 6, 2188000, 18.95, 124.24, 1.92, 0),
  ("Tensor", 1197000, 4, 400000, 14.5, 128.64, 1.54, 0),
  ("Tensor", 1197000, 4, 553000, 14.41, 126.94, 1.58, 0),
  ("Tensor", 1197000, 4, 696000, 14.43, 123.96, 1.63, 0),
  ("Tensor", 1197000, 4, 799000, 14.39, 125.32, 1.59, 0),
  ("Tensor", 1197000, 4, 910000, 14.42, 126.37, 1.55, 0),
  ("Tensor", 1197000, 4, 1024000, 14.5, 110.43, 1.54, 0),
  ("Tensor", 1197000, 4, 1197000, 19.0, 121.68, 1.9, 222.0),
  ("Tensor", 1197000, 4, 1328000, 18.88, 122.27, 1.96, 0),
  ("Tensor", 1197000, 4, 1491000, 18.84, 118.62, 1.98, 0),
  ("Tensor", 1197000, 4, 1663000, 35.35, 175.31, 3.32, 0),
  ("Tensor", 1197000, 4, 1836000, 35.37, 178.17, 3.38, 0),
  ("Tensor", 1197000, 4, 1999000, 35.34, 186.68, 3.38, 0),
  ("Tensor", 1197000, 4, 2130000, 35.37, 176.06, 3.34, 0),
  ("Tensor", 1197000, 4, 2253000, 35.29, 169.24, 3.38, 111.0),
  ("Tensor", 1197000, 6, 500000, 14.47, 95.77, 1.55, 0),
  ("Tensor", 1197000, 6, 851000, 14.42, 101.17, 1.6, 0),
  ("Tensor", 1197000, 6, 984000, 14.21, 116.52, 1.68, 0),
  ("Tensor", 1197000, 6, 1106000, 14.32, 111.16, 1.62, 0),
  ("Tensor", 1197000, 6, 1277000, 14.42, 84.46, 1.6, 0),
  ("Tensor", 1197000, 6, 1426000, 18.83, 130.44, 2.01, 0),
  ("Tensor", 1197000, 6, 1582000, 18.98, 140.9, 1.9, 0),
  ("Tensor", 1197000, 6, 1745000, 18.82, 143.87, 1.94, 0),
  ("Tensor", 1197000, 6, 1826000, 18.91, 131.75, 1.96, 0),
  ("Tensor", 1197000, 6, 2048000, 18.99, 128.36, 1.96, 0),
  ("Tensor", 1197000, 6, 2188000, 18.71, 132.46, 2.07, 0),
  ("Tensor", 1197000, 6, 2252000, 18.82, 130.95, 2.0, 0),
  ("Tensor", 1328000, 4, 400000, 17.0, 135.89, 1.84, 0),
  ("Tensor", 1328000, 4, 553000, 17.1, 161.84, 1.78, 0),
  ("Tensor", 1328000, 4, 696000, 16.99, 142.03, 1.87, 0),
  ("Tensor", 1328000, 4, 799000, 17.07, 169.36, 1.83, 0),
  ("Tensor", 1328000, 4, 910000, 17.19, 111.73, 1.81, 0),
  ("Tensor", 1328000, 4, 1024000, 17.21, 128.66, 1.78, 0),
  ("Tensor", 1328000, 4, 1197000, 18.83, 129.66, 2.02, 0),
  ("Tensor", 1328000, 4, 1328000, 18.88, 132.55, 1.96, 0),
  ("Tensor", 1328000, 4, 1491000, 18.87, 146.14, 2.0, 0),
  ("Tensor", 1328000, 4, 1663000, 35.43, 185.94, 3.27, 0),
  ("Tensor", 1328000, 4, 1836000, 35.46, 165.55, 3.27, 0),
  ("Tensor", 1328000, 4, 1999000, 35.37, 186.76, 3.29, 0),
  ("Tensor", 1328000, 4, 2130000, 35.35, 207.2, 3.34, 0),
  ("Tensor", 1328000, 4, 2253000, 35.31, 209.73, 3.42, 0),
  ("Tensor", 1328000, 6, 500000, 17.15, 130.76, 1.77, 0),
  ("Tensor", 1328000, 6, 851000, 17.06, 123.6, 1.84, 0),
  ("Tensor", 1328000, 6, 984000, 17.21, 130.23, 1.77, 0),
  ("Tensor", 1328000, 6, 1106000, 17.16, 139.65, 1.84, 0),
  ("Tensor", 1328000, 6, 1277000, 17.14, 123.95, 1.83, 0),
  ("Tensor", 1328000, 6, 1426000, 19.15, 141.04, 1.91, 0),
  ("Tensor", 1328000, 6, 1582000, 19.13, 108.29, 1.91, 0),
  ("Tensor", 1328000, 6, 1745000, 19.12, 133.38, 1.9, 0),
  ("Tensor", 1328000, 6, 1826000, 18.87, 137.51, 2.06, 0),
  ("Tensor", 1328000, 6, 2048000, 19.02, 145.9, 1.96, 0),
  ("Tensor", 1328000, 6, 2188000, 19.06, 129.5, 1.94, 0),
  ("Tensor", 1328000, 6, 2252000, 19.05, 125.72, 1.91, 0),
  ("Tensor", 1328000, 6, 2401000, 35.57, 187.29, 3.33, 0),
  ("Tensor", 1328000, 6, 2507000, 35.38, 213.14, 3.44, 0),
  ("Tensor", 1328000, 6, 2630000, 35.47, 181.15, 3.41, 0),
  ("Tensor", 1401000, 4, 400000, 18.85, 184.12, 2.06, 0),
  ("Tensor", 1401000, 4, 553000, 18.91, 168.23, 1.98, 0),
  ("Tensor", 1401000, 4, 696000, 19.11, 184.69, 1.92, 0),
  ("Tensor", 1401000, 4, 799000, 19.16, 175.13, 1.91, 0),
  ("Tensor", 1401000, 4, 910000, 19.02, 161.7, 1.97, 0),
  ("Tensor", 1401000, 4, 1024000, 18.97, 156.68, 2.01, 0),
  ("Tensor", 1401000, 4, 1197000, 19.07, 155.0, 1.97, 0),
  ("Tensor", 1401000, 4, 1328000, 18.95, 159.64, 1.96, 0),
  ("Tensor", 1401000, 4, 1491000, 19.13, 136.78, 1.95, 0),
  ("Tensor", 1401000, 4, 1663000, 35.67, 186.73, 3.29, 0),
  ("Tensor", 1401000, 4, 1836000, 35.51, 220.26, 3.45, 0),
  ("Tensor", 1401000, 4, 1999000, 35.75, 249.18, 3.3, 0),
  ("Tensor", 1401000, 4, 2130000, 35.65, 217.48, 3.4, 0),
  ("Tensor", 1401000, 4, 2253000, 35.66, 248.9, 3.41, 0),
  ("Tensor", 1401000, 6, 500000, 19.05, 152.39, 1.98, 0),
  ("Tensor", 1401000, 6, 851000, 19.0, 148.12, 2.03, 0),
  ("Tensor", 1401000, 6, 984000, 19.01, 128.71, 2.0, 0),
  ("Tensor", 1401000, 6, 1106000, 18.18, 132.83, 2.01, 0),
  ("Tensor", 1401000, 6, 1277000, 19.07, 138.09, 1.95, 0),
  ("Tensor", 1401000, 6, 1426000, 18.92, 144.69, 2.05, 0),
  ("Tensor", 1401000, 6, 1582000, 18.95, 151.34, 2.05, 0),
  ("Tensor", 1401000, 6, 1745000, 18.98, 152.04, 2.01, 0),
  ("Tensor", 1401000, 6, 1826000, 19.11, 151.71, 1.95, 0),
  ("Tensor", 1401000, 6, 2048000, 19.04, 136.69, 1.98, 0),
  ("Tensor", 1401000, 6, 2188000, 18.97, 152.56, 2.0, 0),
  ("Tensor", 1401000, 6, 2252000, 19.09, 149.02, 1.97, 0),
  ("Tensor", 1401000, 6, 2401000, 35.91, 210.3, 3.23, 0),
  ("Tensor", 1401000, 6, 2507000, 35.64, 188.64, 3.32, 0),
  ("Tensor", 1401000, 6, 2630000, 35.41, 202.75, 3.5, 0),
  ("Tensor", 1401000, 6, 2704000, 35.69, 204.49, 3.4, 0),
  ("Tensor", 1401000, 6, 2802000, 35.64, 208.14, 3.45, 0),
  ("Tensor", 1598000, 4, 400000, 24.83, 196.05, 2.36, 0),
  ("Tensor", 1598000, 4, 553000, 24.68, 234.53, 2.37, 0),
  ("Tensor", 1598000, 4, 696000, 24.71, 230.15, 2.34, 0),
  ("Tensor", 1598000, 4, 799000, 24.87, 175.64, 2.34, 0),
  ("Tensor", 1598000, 4, 910000, 24.76, 228.23, 2.36, 0),
  ("Tensor", 1598000, 4, 1024000, 24.6, 228.37, 2.47, 0),
  ("Tensor", 1598000, 4, 1197000, 24.77, 201.12, 2.43, 0),
  ("Tensor", 1598000, 4, 1328000, 24.68, 202.37, 2.41, 0),
  ("Tensor", 1598000, 4, 1491000, 24.58, 199.78, 2.52, 0),
  ("Tensor", 1598000, 4, 1663000, 35.59, 210.2, 3.46, 0),
  ("Tensor", 1598000, 4, 1836000, 35.74, 315.02, 3.33, 0),
  ("Tensor", 1598000, 4, 1999000, 35.65, 285.37, 3.44, 0),
  ("Tensor", 1598000, 4, 2130000, 35.31, 256.84, 3.7, 0),
  ("Tensor", 1598000, 4, 2253000, 35.91, 255.65, 3.37, 0),
  ("Tensor", 1598000, 6, 500000, 24.78, 184.21, 2.34, 0),
  ("Tensor", 1598000, 6, 851000, 24.73, 175.69, 2.41, 0),
  ("Tensor", 1598000, 6, 984000, 24.68, 195.14, 2.43, 0),
  ("Tensor", 1598000, 6, 1106000, 24.65, 194.89, 2.46, 0),
  ("Tensor", 1598000, 6, 1277000, 24.63, 167.1, 2.49, 0),
  ("Tensor", 1598000, 6, 1426000, 24.7, 190.42, 2.45, 0),
  ("Tensor", 1598000, 6, 1582000, 24.79, 190.72, 2.39, 0),
  ("Tensor", 1598000, 6, 1745000, 24.73, 180.52, 2.44, 0),
  ("Tensor", 1598000, 6, 1826000, 24.72, 203.15, 2.4, 0),
  ("Tensor", 1598000, 6, 2048000, 24.82, 197.7, 2.39, 0),
  ("Tensor", 1598000, 6, 2188000, 24.7, 185.45, 2.47, 0),
  ("Tensor", 1598000, 6, 2252000, 24.83, 155.38, 2.35, 0),
  ("Tensor", 1598000, 6, 2401000, 36.0, 237.12, 3.25, 0),
  ("Tensor", 1598000, 6, 2507000, 35.89, 253.55, 3.34, 0),
  ("Tensor", 1598000, 6, 2630000, 35.76, 208.38, 3.45, 0),
  ("Tensor", 1598000, 6, 2704000, 35.7, 218.73, 3.46, 0),
  ("Tensor", 1598000, 6, 2802000, 35.65, 248.51, 3.47, 0),
  ("Tensor", 1704000, 4, 400000, 28.98, 234.84, 2.73, 0),
  ("Tensor", 1704000, 4, 553000, 29.01, 210.31, 2.66, 0),
  ("Tensor", 1704000, 4, 696000, 28.95, 300.74, 2.73, 0),
  ("Tensor", 1704000, 4, 799000, 28.77, 270.96, 2.79, 0),
  ("Tensor", 1704000, 4, 910000, 28.84, 284.84, 2.76, 0),
  ("Tensor", 1704000, 4, 1024000, 28.76, 251.86, 2.85, 0),
  ("Tensor", 1704000, 4, 1197000, 28.75, 256.3, 2.78, 0),
  ("Tensor", 1704000, 4, 1328000, 28.65, 246.88, 2.86, 0),
  ("Tensor", 1704000, 4, 1491000, 28.73, 267.07, 2.88, 0),
  ("Tensor", 1704000, 4, 1663000, 35.81, 266.03, 3.49, 0),
  ("Tensor", 1704000, 4, 1836000, 35.78, 274.06, 3.35, 0),
  ("Tensor", 1704000, 4, 1999000, 35.67, 268.14, 3.46, 0),
  ("Tensor", 1704000, 4, 2130000, 35.75, 273.4, 3.41, 0),
  ("Tensor", 1704000, 4, 2253000, 35.42, 276.92, 3.72, 0),
  ("Tensor", 1704000, 6, 500000, 29.1, 239.74, 2.65, 0),
  ("Tensor", 1704000, 6, 851000, 28.79, 216.53, 2.74, 0),
  ("Tensor", 1704000, 6, 984000, 28.9, 259.03, 2.76, 0),
  ("Tensor", 1704000, 6, 1106000, 28.71, 211.76, 2.82, 0),
  ("Tensor", 1704000, 6, 1277000, 28.79, 216.77, 2.8, 0),
  ("Tensor", 1704000, 6, 1426000, 28.94, 207.8, 2.71, 0),
  ("Tensor", 1704000, 6, 1582000, 28.96, 232.83, 2.67, 0),
  ("Tensor", 1704000, 6, 1745000, 28.67, 237.37, 2.85, 0),
  ("Tensor", 1704000, 6, 1826000, 29.0, 224.71, 2.71, 0),
  ("Tensor", 1704000, 6, 2048000, 28.86, 239.69, 2.73, 0),
  ("Tensor", 1704000, 6, 2188000, 28.88, 218.8, 2.76, 0),
  ("Tensor", 1704000, 6, 2252000, 28.87, 272.23, 2.76, 0),
  ("Tensor", 1704000, 6, 2401000, 35.74, 258.98, 3.33, 0),
  ("Tensor", 1704000, 6, 2507000, 35.74, 276.92, 3.4, 0),
  ("Tensor", 1704000, 6, 2630000, 35.71, 249.7, 3.45, 0),
  ("Tensor", 1704000, 6, 2704000, 36.01, 253.04, 3.29, 0),
  ("Tensor", 1704000, 6, 2802000, 35.91, 266.15, 3.4, 0),
  ("Tensor", 1803000, 4, 400000, 35.71, 342.95, 3.49, 0),
  ("Tensor", 1803000, 4, 553000, 35.76, 330.57, 3.41, 0),
  ("Tensor", 1803000, 4, 696000, 35.71, 355.0, 3.41, 0),
  ("Tensor", 1803000, 4, 799000, 35.67, 310.42, 3.45, 0),
  ("Tensor", 1803000, 4, 910000, 35.95, 309.22, 3.38, 0),
  ("Tensor", 1803000, 4, 1024000, 35.6, 303.94, 3.55, 0),
  ("Tensor", 1803000, 4, 1197000, 36.0, 346.31, 3.26, 0),
  ("Tensor", 1803000, 4, 1328000, 35.9, 300.16, 3.36, 0),
  ("Tensor", 1803000, 4, 1491000, 35.88, 215.33, 3.33, 0),
  ("Tensor", 1803000, 4, 1663000, 35.72, 284.35, 3.47, 0),
  ("Tensor", 1803000, 4, 1836000, 35.9, 289.0, 3.32, 0),
  ("Tensor", 1803000, 4, 1999000, 34.96, 293.38, 3.33, 0),
  ("Tensor", 1803000, 4, 2130000, 35.07, 359.86, 3.19, 0),
  ("Tensor", 1803000, 4, 2253000, 35.07, 295.24, 3.23, 0),
  ("Tensor", 1803000, 6, 500000, 34.68, 223.89, 3.4, 0),
  ("Tensor", 1803000, 6, 851000, 34.74, 261.39, 3.4, 0),
  ("Tensor", 1803000, 6, 984000, 35.08, 269.51, 3.26, 0),
  ("Tensor", 1803000, 6, 1106000, 35.06, 269.58, 3.21, 0),
  ("Tensor", 1803000, 6, 1277000, 34.87, 218.3, 3.39, 0),
  ("Tensor", 1803000, 6, 1426000, 34.86, 264.34, 3.36, 0),
  ("Tensor", 1803000, 6, 1582000, 34.9, 263.56, 3.36, 0),
  ("Tensor", 1803000, 6, 1745000, 35.09, 210.36, 3.29, 0),
  ("Tensor", 1803000, 6, 1826000, 35.06, 256.1, 3.34, 0),
  ("Tensor", 1803000, 6, 2048000, 35.18, 269.91, 3.16, 0),
  ("Tensor", 1803000, 6, 2188000, 35.16, 261.04, 3.25, 0),
  ("Tensor", 1803000, 6, 2252000, 34.84, 272.92, 3.49, 0),
  ("Tensor", 1803000, 6, 2401000, 35.2, 260.24, 3.38, 0),
  ("Tensor", 1803000, 6, 2507000, 34.89, 240.7, 3.58, 0),
  ("Tensor", 1803000, 6, 2630000, 35.21, 150.76, 3.42, 0),
  ("Tensor", 1803000, 6, 2704000, 35.2, 277.28, 3.44, 0),
  ("Tensor", 1803000, 6, 2802000, 35.12, 269.2, 3.62, 0)
)
select * from data;

CREATE PERFETTO TABLE _device_curves_l3
AS
WITH data(device, freq_khz, other_policy, other_freq_khz, l3_hit, l3_miss) AS (
  VALUES
  ("Tensor", 300000, 4, 400000, 0.3989, 0.0629),
  ("Tensor", 300000, 4, 553000, 0.4119, 0.0656),
  ("Tensor", 300000, 6, 500000, 0.3298, 0.1029),
  ("Tensor", 574000, 4, 400000, 0.4894, 0.0239),
  ("Tensor", 574000, 4, 553000, 0.4991, 0.0960),
  ("Tensor", 574000, 4, 696000, 0.4949, 0.0971),
  ("Tensor", 574000, 4, 799000, 0.6116, 0.1266),
  ("Tensor", 574000, 4, 910000, 0.5897, 0.1385),
  ("Tensor", 574000, 4, 1024000, 0.5619, 0.0635),
  ("Tensor", 574000, 6, 500000, 0.5377, 0.1210),
  ("Tensor", 574000, 6, 851000, 0.5271, 0.1591),
  ("Tensor", 574000, 6, 984000, 0.5395, 0.1599),
  ("Tensor", 574000, 6, 1106000, 0.5552, 0.1393),
  ("Tensor", 738000, 4, 400000, 0.5825, 0.1271),
  ("Tensor", 738000, 4, 553000, 0.5751, 0.0396),
  ("Tensor", 738000, 4, 696000, 0.6433, 0.1050),
  ("Tensor", 738000, 4, 799000, 0.6401, 0.1293),
  ("Tensor", 738000, 4, 910000, 0.7069, 0.1252),
  ("Tensor", 738000, 4, 1024000, 0.6999, 0.1143),
  ("Tensor", 738000, 4, 1197000, 0.9076, 0.1960),
  ("Tensor", 738000, 4, 1328000, 0.9708, 0.1953),
  ("Tensor", 738000, 6, 500000, 0.6437, 0.2086),
  ("Tensor", 738000, 6, 851000, 0.6274, 0.1852),
  ("Tensor", 738000, 6, 984000, 0.6231, 0.2066),
  ("Tensor", 738000, 6, 1106000, 0.6256, 0.2199),
  ("Tensor", 738000, 6, 1277000, 0.6719, 0.2485),
  ("Tensor", 738000, 6, 1426000, 1.1072, 0.3483),
  ("Tensor", 930000, 4, 400000, 0.7812, 0.1727),
  ("Tensor", 930000, 4, 553000, 0.7343, 0.1846),
  ("Tensor", 930000, 4, 696000, 0.7551, 0.2006),
  ("Tensor", 930000, 4, 799000, 0.7330, 0.1864),
  ("Tensor", 930000, 4, 910000, 0.8250, 0.1451),
  ("Tensor", 930000, 4, 1024000, 0.7331, 0.2092),
  ("Tensor", 930000, 4, 1197000, 1.0791, 0.4804),
  ("Tensor", 930000, 4, 1328000, 1.0172, 0.0844),
  ("Tensor", 930000, 4, 1491000, 1.0396, 0.2614),
  ("Tensor", 930000, 4, 1663000, 1.6492, 0.3497),
  ("Tensor", 930000, 4, 1836000, 1.5561, 0.3407),
  ("Tensor", 930000, 6, 500000, 0.8530, 0.4182),
  ("Tensor", 930000, 6, 851000, 0.8694, 0.2854),
  ("Tensor", 930000, 6, 984000, 0.8620, 0.2568),
  ("Tensor", 930000, 6, 1106000, 0.8763, 0.2336),
  ("Tensor", 930000, 6, 1277000, 0.8717, 0.3756),
  ("Tensor", 930000, 6, 1426000, 1.1774, 0.5021),
  ("Tensor", 930000, 6, 1582000, 1.1264, 0.5799),
  ("Tensor", 930000, 6, 1745000, 1.2303, 0.5421),
  ("Tensor", 930000, 6, 1826000, 1.2330, 0.4498),
  ("Tensor", 1098000, 4, 400000, 0.9744, 0.2106),
  ("Tensor", 1098000, 4, 553000, 0.9980, 0.0500),
  ("Tensor", 1098000, 4, 696000, 0.9500, 0.1928),
  ("Tensor", 1098000, 4, 799000, 0.9132, 0.2391),
  ("Tensor", 1098000, 4, 910000, 0.9922, 0.2576),
  ("Tensor", 1098000, 4, 1024000, 0.9607, 0.2397),
  ("Tensor", 1098000, 4, 1197000, 1.1253, 0.6195),
  ("Tensor", 1098000, 4, 1328000, 1.1609, 0.0960),
  ("Tensor", 1098000, 4, 1491000, 1.1783, 0.0851),
  ("Tensor", 1098000, 4, 1663000, 1.6941, 0.4295),
  ("Tensor", 1098000, 4, 1836000, 1.7152, 0.4610),
  ("Tensor", 1098000, 4, 1999000, 1.7941, 0.4293),
  ("Tensor", 1098000, 4, 2130000, 1.6758, 0.4437),
  ("Tensor", 1098000, 6, 500000, 1.0485, 0.4038),
  ("Tensor", 1098000, 6, 851000, 1.0510, 0.2815),
  ("Tensor", 1098000, 6, 984000, 1.0785, 0.4137),
  ("Tensor", 1098000, 6, 1106000, 1.0909, 0.3933),
  ("Tensor", 1098000, 6, 1277000, 1.1533, 0.3811),
  ("Tensor", 1098000, 6, 1426000, 1.2718, 0.3814),
  ("Tensor", 1098000, 6, 1582000, 1.3463, 0.4100),
  ("Tensor", 1098000, 6, 1745000, 1.3065, 0.5207),
  ("Tensor", 1098000, 6, 1826000, 1.3456, 0.4903),
  ("Tensor", 1098000, 6, 2048000, 1.3466, 0.7218),
  ("Tensor", 1098000, 6, 2188000, 1.3132, 0.4923),
  ("Tensor", 1197000, 4, 400000, 1.0507, 0.2411),
  ("Tensor", 1197000, 4, 553000, 1.0387, 0.2875),
  ("Tensor", 1197000, 4, 696000, 1.0173, 0.2232),
  ("Tensor", 1197000, 4, 799000, 1.0160, 0.2418),
  ("Tensor", 1197000, 4, 910000, 1.0555, 0.0966),
  ("Tensor", 1197000, 4, 1024000, 1.0663, 0.0987),
  ("Tensor", 1197000, 4, 1197000, 1.1885, 0.2852),
  ("Tensor", 1197000, 4, 1328000, 1.2442, 0.2724),
  ("Tensor", 1197000, 4, 1491000, 1.2474, 0.3269),
  ("Tensor", 1197000, 4, 1663000, 1.8142, 0.3429),
  ("Tensor", 1197000, 4, 1836000, 1.7692, 1.0737),
  ("Tensor", 1197000, 4, 1999000, 1.7939, 0.1120),
  ("Tensor", 1197000, 4, 2130000, 1.8126, 0.3744),
  ("Tensor", 1197000, 4, 2253000, 1.7413, 0.5198),
  ("Tensor", 1197000, 6, 500000, 1.1288, 0.6817),
  ("Tensor", 1197000, 6, 851000, 1.1779, 0.5681),
  ("Tensor", 1197000, 6, 984000, 1.1835, 0.3389),
  ("Tensor", 1197000, 6, 1106000, 1.2115, 0.4506),
  ("Tensor", 1197000, 6, 1277000, 1.1726, 0.8719),
  ("Tensor", 1197000, 6, 1426000, 1.3825, 0.5140),
  ("Tensor", 1197000, 6, 1582000, 1.4179, 0.3585),
  ("Tensor", 1197000, 6, 1745000, 1.3804, 0.3197),
  ("Tensor", 1197000, 6, 1826000, 1.3379, 0.5614),
  ("Tensor", 1197000, 6, 2048000, 1.3335, 0.5443),
  ("Tensor", 1197000, 6, 2188000, 1.4382, 0.5255),
  ("Tensor", 1197000, 6, 2252000, 1.3961, 0.5423),
  ("Tensor", 1328000, 4, 400000, 1.2307, 0.5565),
  ("Tensor", 1328000, 4, 553000, 1.2186, 0.2366),
  ("Tensor", 1328000, 4, 696000, 1.2243, 0.4145),
  ("Tensor", 1328000, 4, 799000, 1.2620, 0.0973),
  ("Tensor", 1328000, 4, 910000, 1.2462, 0.5669),
  ("Tensor", 1328000, 4, 1024000, 1.2787, 0.2332),
  ("Tensor", 1328000, 4, 1197000, 1.4364, 0.3260),
  ("Tensor", 1328000, 4, 1328000, 1.3636, 0.3354),
  ("Tensor", 1328000, 4, 1491000, 1.3733, 0.0512),
  ("Tensor", 1328000, 4, 1663000, 1.9295, 0.4588),
  ("Tensor", 1328000, 4, 1836000, 1.8278, 0.9316),
  ("Tensor", 1328000, 4, 1999000, 1.9043, 0.4921),
  ("Tensor", 1328000, 4, 2130000, 1.9144, 0.1139),
  ("Tensor", 1328000, 4, 2253000, 1.9550, 0.0603),
  ("Tensor", 1328000, 6, 500000, 1.3772, 0.5737),
  ("Tensor", 1328000, 6, 851000, 1.3985, 0.6368),
  ("Tensor", 1328000, 6, 984000, 1.3933, 0.5311),
  ("Tensor", 1328000, 6, 1106000, 1.3932, 0.4567),
  ("Tensor", 1328000, 6, 1277000, 1.3984, 0.6616),
  ("Tensor", 1328000, 6, 1426000, 1.5067, 0.5776),
  ("Tensor", 1328000, 6, 1582000, 1.5167, 1.0309),
  ("Tensor", 1328000, 6, 1745000, 1.5021, 0.6845),
  ("Tensor", 1328000, 6, 1826000, 1.4775, 0.6285),
  ("Tensor", 1328000, 6, 2048000, 1.5237, 0.5402),
  ("Tensor", 1328000, 6, 2188000, 1.5349, 0.7490),
  ("Tensor", 1328000, 6, 2252000, 1.5436, 0.7984),
  ("Tensor", 1328000, 6, 2401000, 2.1755, 1.0387),
  ("Tensor", 1328000, 6, 2507000, 2.2320, 0.7382),
  ("Tensor", 1328000, 6, 2630000, 2.2489, 1.1762),
  ("Tensor", 1401000, 4, 400000, 1.3279, 0.2793),
  ("Tensor", 1401000, 4, 553000, 1.3065, 0.3853),
  ("Tensor", 1401000, 4, 696000, 1.3290, 0.3016),
  ("Tensor", 1401000, 4, 799000, 1.2483, 0.3683),
  ("Tensor", 1401000, 4, 910000, 1.4059, 0.2825),
  ("Tensor", 1401000, 4, 1024000, 1.3702, 0.3389),
  ("Tensor", 1401000, 4, 1197000, 1.3920, 0.3614),
  ("Tensor", 1401000, 4, 1328000, 1.3752, 0.3310),
  ("Tensor", 1401000, 4, 1491000, 1.4015, 0.6546),
  ("Tensor", 1401000, 4, 1663000, 1.8982, 1.0324),
  ("Tensor", 1401000, 4, 1836000, 1.9447, 0.5336),
  ("Tensor", 1401000, 4, 1999000, 2.1219, 0.0662),
  ("Tensor", 1401000, 4, 2130000, 1.9576, 0.5584),
  ("Tensor", 1401000, 4, 2253000, 2.0221, 0.1254),
  ("Tensor", 1401000, 6, 500000, 1.5283, 0.5764),
  ("Tensor", 1401000, 6, 851000, 1.5211, 0.5643),
  ("Tensor", 1401000, 6, 984000, 1.5574, 0.7558),
  ("Tensor", 1401000, 6, 1106000, 1.5492, 0.7862),
  ("Tensor", 1401000, 6, 1277000, 1.5389, 0.7523),
  ("Tensor", 1401000, 6, 1426000, 1.6449, 0.5993),
  ("Tensor", 1401000, 6, 1582000, 1.5953, 0.5512),
  ("Tensor", 1401000, 6, 1745000, 1.5672, 0.5489),
  ("Tensor", 1401000, 6, 1826000, 1.5639, 0.5507),
  ("Tensor", 1401000, 6, 2048000, 1.5878, 0.7536),
  ("Tensor", 1401000, 6, 2188000, 1.5562, 0.5431),
  ("Tensor", 1401000, 6, 2252000, 1.5908, 0.6087),
  ("Tensor", 1401000, 6, 2401000, 2.2693, 0.8953),
  ("Tensor", 1401000, 6, 2507000, 2.3182, 1.2289),
  ("Tensor", 1401000, 6, 2630000, 2.3090, 1.0687),
  ("Tensor", 1401000, 6, 2704000, 2.2751, 0.9966),
  ("Tensor", 1401000, 6, 2802000, 2.3278, 0.9065),
  ("Tensor", 1598000, 4, 400000, 1.7424, 0.8926),
  ("Tensor", 1598000, 4, 553000, 1.7003, 0.4482),
  ("Tensor", 1598000, 4, 696000, 1.6099, 0.5281),
  ("Tensor", 1598000, 4, 799000, 1.8018, 0.9634),
  ("Tensor", 1598000, 4, 910000, 1.7615, 0.3445),
  ("Tensor", 1598000, 4, 1024000, 1.7317, 0.3396),
  ("Tensor", 1598000, 4, 1197000, 1.7293, 0.5079),
  ("Tensor", 1598000, 4, 1328000, 1.8771, 0.4685),
  ("Tensor", 1598000, 4, 1491000, 1.8724, 0.4693),
  ("Tensor", 1598000, 4, 1663000, 1.9587, 1.2295),
  ("Tensor", 1598000, 4, 1836000, 2.2287, 0.5220),
  ("Tensor", 1598000, 4, 1999000, 2.1786, 0.1494),
  ("Tensor", 1598000, 4, 2130000, 2.1631, 0.4924),
  ("Tensor", 1598000, 4, 2253000, 2.1703, 0.5427),
  ("Tensor", 1598000, 6, 500000, 1.9632, 0.9534),
  ("Tensor", 1598000, 6, 851000, 1.9820, 0.9433),
  ("Tensor", 1598000, 6, 984000, 1.9745, 0.8002),
  ("Tensor", 1598000, 6, 1106000, 1.9514, 0.8323),
  ("Tensor", 1598000, 6, 1277000, 1.9796, 1.1016),
  ("Tensor", 1598000, 6, 1426000, 1.9432, 0.8556),
  ("Tensor", 1598000, 6, 1582000, 2.0700, 0.8211),
  ("Tensor", 1598000, 6, 1745000, 2.0052, 0.9492),
  ("Tensor", 1598000, 6, 1826000, 2.0165, 0.7016),
  ("Tensor", 1598000, 6, 2048000, 2.0881, 0.6641),
  ("Tensor", 1598000, 6, 2188000, 2.1239, 0.8702),
  ("Tensor", 1598000, 6, 2252000, 2.0952, 1.1728),
  ("Tensor", 1598000, 6, 2401000, 2.4810, 0.9498),
  ("Tensor", 1598000, 6, 2507000, 2.4644, 0.9131),
  ("Tensor", 1598000, 6, 2630000, 2.4030, 1.3728),
  ("Tensor", 1598000, 6, 2704000, 2.4271, 1.2680),
  ("Tensor", 1598000, 6, 2802000, 2.4761, 0.9789),
  ("Tensor", 1704000, 4, 400000, 1.9466, 0.9753),
  ("Tensor", 1704000, 4, 553000, 1.9336, 1.0846),
  ("Tensor", 1704000, 4, 696000, 1.9280, 0.2116),
  ("Tensor", 1704000, 4, 799000, 1.9616, 0.4219),
  ("Tensor", 1704000, 4, 910000, 1.9627, 0.1957),
  ("Tensor", 1704000, 4, 1024000, 1.9763, 0.5599),
  ("Tensor", 1704000, 4, 1197000, 1.9514, 0.4326),
  ("Tensor", 1704000, 4, 1328000, 2.0093, 0.4861),
  ("Tensor", 1704000, 4, 1491000, 1.9438, 0.1584),
  ("Tensor", 1704000, 4, 1663000, 2.3012, 0.6019),
  ("Tensor", 1704000, 4, 1836000, 2.2896, 0.5019),
  ("Tensor", 1704000, 4, 1999000, 2.2292, 0.6076),
  ("Tensor", 1704000, 4, 2130000, 2.2087, 0.5726),
  ("Tensor", 1704000, 4, 2253000, 2.2317, 0.4878),
  ("Tensor", 1704000, 6, 500000, 2.3606, 0.7822),
  ("Tensor", 1704000, 6, 851000, 2.2564, 0.9656),
  ("Tensor", 1704000, 6, 984000, 2.2618, 0.9988),
  ("Tensor", 1704000, 6, 1106000, 2.2796, 0.9681),
  ("Tensor", 1704000, 6, 1277000, 2.2224, 0.8812),
  ("Tensor", 1704000, 6, 1426000, 2.2368, 1.0353),
  ("Tensor", 1704000, 6, 1582000, 2.3125, 0.8402),
  ("Tensor", 1704000, 6, 1745000, 2.3199, 0.7728),
  ("Tensor", 1704000, 6, 1826000, 2.3633, 0.8597),
  ("Tensor", 1704000, 6, 2048000, 2.2779, 0.6885),
  ("Tensor", 1704000, 6, 2188000, 2.2575, 1.0289),
  ("Tensor", 1704000, 6, 2252000, 2.2798, 0.9689),
  ("Tensor", 1704000, 6, 2401000, 2.5202, 1.0626),
  ("Tensor", 1704000, 6, 2507000, 2.4070, 0.8463),
  ("Tensor", 1704000, 6, 2630000, 2.5998, 1.0795),
  ("Tensor", 1704000, 6, 2704000, 2.6273, 1.0329),
  ("Tensor", 1704000, 6, 2802000, 2.6179, 0.7569),
  ("Tensor", 1803000, 4, 400000, 2.2197, 0.4673),
  ("Tensor", 1803000, 4, 553000, 2.3144, 0.5120),
  ("Tensor", 1803000, 4, 696000, 2.2720, 0.1952),
  ("Tensor", 1803000, 4, 799000, 2.3472, 0.5479),
  ("Tensor", 1803000, 4, 910000, 2.3035, 0.5622),
  ("Tensor", 1803000, 4, 1024000, 2.2129, 0.6828),
  ("Tensor", 1803000, 4, 1197000, 2.3176, 0.1645),
  ("Tensor", 1803000, 4, 1328000, 2.3127, 0.4992),
  ("Tensor", 1803000, 4, 1491000, 2.1449, 1.4705),
  ("Tensor", 1803000, 4, 1663000, 2.3243, 0.6256),
  ("Tensor", 1803000, 4, 1836000, 2.1328, 0.6293),
  ("Tensor", 1803000, 4, 1999000, 2.3165, 0.5265),
  ("Tensor", 1803000, 4, 2130000, 2.2775, 0.6412),
  ("Tensor", 1803000, 4, 2253000, 2.4124, 0.5151),
  ("Tensor", 1803000, 6, 500000, 2.5536, 1.5678),
  ("Tensor", 1803000, 6, 851000, 2.5831, 1.1737),
  ("Tensor", 1803000, 6, 984000, 2.6063, 1.0591),
  ("Tensor", 1803000, 6, 1106000, 2.6951, 0.9158),
  ("Tensor", 1803000, 6, 1277000, 2.5400, 1.5096),
  ("Tensor", 1803000, 6, 1426000, 2.6623, 1.1037),
  ("Tensor", 1803000, 6, 1582000, 2.6996, 1.0774),
  ("Tensor", 1803000, 6, 1745000, 2.6692, 1.6543),
  ("Tensor", 1803000, 6, 1826000, 2.7288, 1.1255),
  ("Tensor", 1803000, 6, 2048000, 2.6649, 1.1010),
  ("Tensor", 1803000, 6, 2188000, 2.6489, 1.1485),
  ("Tensor", 1803000, 6, 2252000, 2.6389, 1.0942),
  ("Tensor", 1803000, 6, 2401000, 2.6256, 1.0997),
  ("Tensor", 1803000, 6, 2507000, 2.6630, 1.2641),
  ("Tensor", 1803000, 6, 2630000, 2.7385, 2.3263),
  ("Tensor", 1803000, 6, 2704000, 2.6901, 1.0629),
  ("Tensor", 1803000, 6, 2802000, 2.7476, 1.0673)
)
select * from data;
