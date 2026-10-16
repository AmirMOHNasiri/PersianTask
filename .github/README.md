<div dir="rtl">

# PersianTask

**Taskwarrior با تقویم شمسی (جلالی)**

PersianTask نسخه‌ای از [Taskwarrior](https://taskwarrior.org) است که در آن همهٔ تاریخ‌ها، چه هنگام وارد کردن و چه هنگام نمایش، شمسی هستند. بقیهٔ قابلیت‌های Taskwarrior بدون تغییر باقی مانده‌اند و آپدیت‌های نسخهٔ اصلی هر هفته به‌طور خودکار، پس از گذراندن تست‌های تقویم شمسی، به این نسخه اضافه می‌شوند.

```
$ task add jalase due:1405-08-01
$ task add gozaresh due:eom
$ task calendar

         Mehr 1405                 Aban 1405                 Azar 1405
     Sh Ye Do Se Ch Pa Jo      Sh Ye Do Se Ch Pa Jo      Sh Ye Do Se Ch Pa Jo
  27              1  2  3   31                    1   36     1  2  3  4  5  6
  28  4  5  6  7  8  9 10   32  2  3  4  5  6  7  8   37  7  8  9 10 11 12 13
  ...
```

## نصب

### آرچ لینوکس (و Manjaro، EndeavourOS و ...)

```
mkdir persiantask && cd persiantask
curl -O https://raw.githubusercontent.com/AmirMOHNasiri/PersianTask/main/pkg/arch/PKGBUILD
makepkg -si
```

اگر `task` رسمی نصب باشد، هنگام نصب پیشنهاد حذف آن داده می‌شود. برای آپدیت، دوباره در همان پوشه `makepkg -si` را اجرا کنید.

### توزیع‌های دیگر لینوکس

ابزارهای لازم: `git`، `cmake`، `g++`، کتابخانهٔ `libuuid` (در اوبونتو و دبیان `uuid-dev`) و [Rust](https://rustup.rs).

```
git clone --recursive https://github.com/AmirMOHNasiri/PersianTask.git
cd PersianTask
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

اگر Taskwarrior رسمی را از مخزن توزیع نصب کرده‌اید، ابتدا آن را حذف کنید.

## استفاده

همهٔ دستورهای Taskwarrior مثل قبل کار می‌کنند؛ فقط تاریخ‌ها شمسی‌اند.

| ورودی | معنی |
|---|---|
| `due:1405-08-01` | یکم آبان ۱۴۰۵ |
| `due:۱۴۰۵-۰۸-۰۱` | همان، با اعداد فارسی |
| `due:esfand` | اول اسفند بعدی |
| `due:shanbe` | شنبهٔ بعدی |
| `due:15th` | پانزدهم ماه (این ماه یا ماه بعد) |
| `due:sow` / `due:eow` | شروع / پایان این هفته (شنبه تا جمعه) |
| `due:soww` / `due:eoww` | شروع / پایان هفتهٔ کاری (شنبه تا چهارشنبه) |
| `due:som` / `due:eom` | شروع / پایان این ماه شمسی |
| `due:soq` / `due:eoq` | شروع / پایان این فصل |
| `due:soy` / `due:eoy` | شروع / پایان این سال شمسی |
| `recur:monthly` | تکرار ماهانه، بر اساس ماه شمسی |
| `recur:weekdays` | تکرار در روزهای کاری (شنبه تا چهارشنبه) |

نام ماه‌ها: Farvardin، Ordibehesht، Khordad، Tir، Mordad، Shahrivar، Mehr، Aban، Azar، Dey، Bahman، Esfand

نام روزها: Shanbe، Yekshanbe، Doshanbe، Seshanbe، Chaharshanbe، Panjshanbe، Jome (نام انگلیسی روزها مثل `friday` هم پذیرفته می‌شود)

نام‌ها را می‌توان کوتاه نوشت (حداقل سه حرف)، مثلاً `due:far` یا `due:pan`. چون `sha` به شنبه می‌خورد، برای شهریور دست‌کم `shah` بنویسید.

قالب نمایش تاریخ را می‌توانید با `dateformat` تغییر دهید. مثلاً با `task config dateformat 'A d B Y'` تاریخ‌ها به شکل `Panjshanbe 30 Mehr 1405` نمایش داده می‌شوند.

## نکته‌ها

- **سازگاری داده‌ها:** تسک‌ها مثل نسخهٔ اصلی ذخیره می‌شوند. اگر از قبل Taskwarrior 3 داشته‌اید، داده‌هایتان بدون تغییر کار می‌کنند و فقط از این به بعد شمسی نمایش داده می‌شوند.
- **قالب‌های ماشینی میلادی‌اند:** خروجی `export`، ورودی `import`، hookها و فایل‌های تعطیلات میلادی می‌مانند تا با ابزارهای دیگر سازگار باشند. هر سالی که ۱۷۰۰ یا بیشتر باشد میلادی در نظر گرفته می‌شود، پس `due:2026-10-16` هم درست کار می‌کند.
- **محدودهٔ تاریخ:** مثل نسخهٔ اصلی، تاریخ‌های پیش از سال ۱۳۵۹ (۱۹۸۰) پذیرفته نمی‌شوند.

## ساختار پروژه

تغییرات PersianTask نسبت به نسخهٔ اصلی:

- `src/libshared/src/Datetime.cpp`: تبدیل تقویم شمسی و میلادی (الگوریتم Borkowski، مطابق تقویم رسمی ایران)
- `src/commands/CmdCalendar.cpp`، `src/recur.cpp` و `src/Context.cpp`: تقویم، تکرار و شروع هفته از شنبه
- `jalali/`: تست‌های تقویم شمسی و اسکریپت ادغام آپدیت‌های نسخهٔ اصلی
- `.github/workflows/jalali-*.yml`: اجرای تست‌ها و همگام‌سازی هفتگی با نسخهٔ اصلی
- `pkg/arch/PKGBUILD`: پکیج آرچ لینوکس

برای اجرای تست‌ها: `bash jalali/build-and-test.sh`

## مجوز

PersianTask مانند Taskwarrior تحت [مجوز MIT](../LICENSE) منتشر می‌شود. Taskwarrior ساختهٔ [GothenburgBitFactory](https://github.com/GothenburgBitFactory/taskwarrior) است. README اصلی پروژه در [README.md](../README.md) قرار دارد.

</div>
