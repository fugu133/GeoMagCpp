# GeoMagCpp

The IGRF model calculates the earth's magnetic field.

# How to use

## 1. Import this library into your project.

All functions are available by including `Core.hpp`.

```C++
#include <GeoMag/Core.hpp>

```
All APIs are in the `geomag` namespace.

## 2. Time Definition

Time operations are performed in the `DateTime` class.
Only ISO-8601 Extended format (YYYYY-MM-DDThh:mm:ss.ssss±hh:mm) is supported as input format.
However, the following degrees of freedom are available.

1. Any single letter may be used for `T` in `<date>T<time>``.
1. There is no restriction on the length of the decimal part `.ssss` only the integer part `ss` is acceptable.
1. If time zone `±hh:mm` is not specified or `Z` is used, it is treated as UTC+0.

```C++
DateTime dt("2000-02-20T02:20:00.00+09:00");
```

### 2.1 Convert time system

Conversion of major time formats is performed as follows.
For example, UNIX time, Julian date, and modified Julian date (based on JD2000.0) are converted as follows

```C++
DateTime dt("2023-12-03T00:00:00");

std::cout << "UNIX" << dt.unixTime() << std::endl;
std::cout << "JD: " << dt.julianDay()  << std::endl;
std::cout << "MJD(JD2000.0): " << dt.j2000()  << std::endl;
```

Other conversions to Greenwich sidereal time, local sidereal time, and earth time (predicted) are also available.

### 2.2 Arithmetic evaluation

Arithmetic operations on time are performed using the dedicated `addXXX`` member function or the `TimeSpan` class.
Standard arithmetic, relational, and equality operators are supported.

```C++
DateTime dt("2023-12-03T00:00:00");

std::cout << dt.addMinutes(1) << std::endl;
std::cout << dt + TimeSpan(1, TimeUnit::Minutes) << std::endl;
std::cout << dt <= DateTime("2024-12-03T00:00:00") << std::endl;
```

## 3 Angle Definition

Angles are represented by the `Angle` class.
This class holds the normalized angles and provides smooth conversion to individual angle units.

Angles can be expressed in arc radians, degrees, hours, arcminutes, and arcseconds.
HMS and DMS formats, which are commonly used in astronomy, are also supported.

```C++
Angle ang_deg = Degree(123.456);
Angle ang_rad = Radian(2.1547136813);
Angle ang_hms = Hms(8, 13, 49.44);
Angle ang_dms = Dms(123, 27, 21.6);
```

### 3.1 Convert angle unit

Angle unit conversion calls a dedicated member.

```C++
Angle ang = Degree(123.456);

std::cout << ang.radians() << std::endl;
std::cout << ang.arcmins() << std::endl;
```

### 3.2 Arithmetic evaluation

Standard arithmetic, relational, and equality operators are supported.
Trigonometric functions can be used without regard to angular units.

```C++
std::cout << Dms(124, 27, 21.6) - Degree(1) << std::endl;
std::cout << Degree(90).sin() << std::endl;
std::cout << Angle::acos(0.5).degrees() << std::endl;
```

### 4. Magnetic flux density calculation of the earth's magnetic field

The `Igrf` class can calculate the magnetic flux density of the earth's magnetic field using the IGRF-13 model.
The ECEF Cartesian coordinate system and the WGS84 rotational ellipsoid coordinate system can be used as position information.

The output flux density vector is in the NED coordinate system and the unit is `T`.

```C++
Igrf igrf;
auto position = Wgs84{DateTime::now(), Degree{135}, Degree{35}, 500e3};
std::cout << igrf(position).transpose() << std::endl;
std::cout << igrf(position.toEcef()).transpose() << std::endl;
```

# Reference

https://www.ncei.noaa.gov/services/world-data-system/v-mod-working-group
https://www.ncei.noaa.gov/products/international-geomagnetic-reference-field/health-warning
https://earth-planets-space.springeropen.com/articles/10.1186/s40623-020-01288-x


# License

MIT License

Copyright (c) 2023 Kaiji Takeuchi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


# SpecialThanks

Some linear algebra operations use [`Eigen`](https://eigen.tuxfamily.org/index.php?title=Main_Page).
