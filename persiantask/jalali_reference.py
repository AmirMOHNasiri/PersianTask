"""Reference Jalali <-> Gregorian conversion, used to check PersianTask.

This is the arithmetic 33-year-cycle algorithm (as popularized by jdf.scr.ir),
which is deliberately independent of the Borkowski/jalaali algorithm used in
src/libshared/src/Datetime.cpp.  The two agree with the official Iranian
calendar for Jalali years 1343-1473 (1964-2094 CE), which is the range tested.
"""

import datetime

FIRST_TESTED_YEAR = 1343
LAST_TESTED_YEAR = 1473


def gregorian_to_jalali(gy, gm, gd):
    g_d_m = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]
    gy2 = gy + 1 if gm > 2 else gy
    days = (355666 + 365 * gy + (gy2 + 3) // 4 - (gy2 + 99) // 100
            + (gy2 + 399) // 400 + gd + g_d_m[gm - 1])
    jy = -1595 + 33 * (days // 12053)
    days %= 12053
    jy += 4 * (days // 1461)
    days %= 1461
    if days > 365:
        jy += (days - 1) // 365
        days = (days - 1) % 365
    if days < 186:
        return jy, 1 + days // 31, 1 + days % 31
    return jy, 7 + (days - 186) // 30, 1 + (days - 186) % 30


def jalali_to_gregorian(jy, jm, jd):
    jy += 1595
    days = (-355668 + 365 * jy + (jy // 33) * 8 + ((jy % 33) + 3) // 4 + jd
            + ((jm - 1) * 31 if jm < 7 else (jm - 7) * 30 + 186))
    gy = 400 * (days // 146097)
    days %= 146097
    if days > 36524:
        days -= 1
        gy += 100 * (days // 36524)
        days %= 36524
        if days >= 365:
            days += 1
    gy += 4 * (days // 1461)
    days %= 1461
    if days > 365:
        gy += (days - 1) // 365
        days = (days - 1) % 365
    gd = days + 1
    leap = (gy % 4 == 0 and gy % 100 != 0) or gy % 400 == 0
    month_days = [0, 31, 29 if leap else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    gm = 0
    while gm < 13 and gd > month_days[gm]:
        gd -= month_days[gm]
        gm += 1
    return gy, gm, gd


def to_jalali(date):
    """Jalali (y, m, d) of a datetime.date."""
    return gregorian_to_jalali(date.year, date.month, date.day)


def from_jalali(jy, jm, jd):
    """datetime.date of a Jalali date.  Months out of 1-12 are normalized."""
    while jm > 12:
        jm -= 12
        jy += 1
    while jm < 1:
        jm += 12
        jy -= 1
    return datetime.date(*jalali_to_gregorian(jy, jm, 1)) + datetime.timedelta(days=jd - 1)


def is_leap(jy):
    """True if the Jalali year has 366 days."""
    return (from_jalali(jy + 1, 1, 1) - from_jalali(jy, 1, 1)).days == 366


def days_in_month(jy, jm):
    if jm <= 6:
        return 31
    if jm <= 11:
        return 30
    return 30 if is_leap(jy) else 29
