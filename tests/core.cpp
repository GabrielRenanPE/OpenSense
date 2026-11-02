#include "../src/system.hpp"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace osense;
static int checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
void put(fs::path p, std::string s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << s;
}
int main() {
  auto root = fs::temp_directory_path() /
              ("opensense-tests-" + std::to_string(getpid()));
  fs::create_directories(root);
  try {
    check(!number("nan"), "NaN rejected");
    check(!number("12bad"), "Malformed number rejected");
    check(number("0") == 0, "Zero preserved");
    auto bat = root / "class/power_supply/BAT1";
    put(bat / "type", "Battery");
    put(bat / "energy_full", "52800000");
    put(bat / "energy_full_design", "57400000");
    auto h = health(bat);
    check(h && std::abs(h->percent - 91.986) < .01, "Health ratio");
    check(h->unit == "Wh", "Energy units");
    put(bat / "energy_full_design", "0");
    check(!health(bat), "Invalid denominator");
    put(bat / "charge_full", "4000000");
    put(bat / "charge_full_design", "5000000");
    check(health(bat)->percent == 80 && health(bat)->unit == "Ah",
          "Charge fallback");
    check(protection(root).path.empty(), "Absent backend");
    put(bat / "charge_control_end_threshold", "80");
    check(protection(root).standard, "Standard backend preferred");
    put(root / "class/power_supply/BAT2/type", "Battery");
    check(protection(root).path.empty(), "Multiple batteries refused");
    fs::remove_all(root / "class/power_supply/BAT2");
    fs::remove(bat / "charge_control_end_threshold");
    put(root / "class/dmi/id/sys_vendor", "Acer");
    auto limiter = root / "module/linuwu_sense/drivers/platform:acer-wmi/"
                          "acer-wmi/nitro_sense/battery_limiter";
    put(limiter, "1");
    check(protection(root).path == limiter && !protection(root).standard,
          "Acer fallback");
    check(commandValue("battery-on", true) == "80" &&
              commandValue("battery-off", true) == "100",
          "Threshold actions");
    check(commandValue("battery-on", false) == "1" &&
              commandValue("battery-off", false) == "0",
          "Limiter actions");
    check(!allowed("gpu-nvidia") && !allowed("/tmp/path") &&
              !allowed("gpu-hybrid; id"),
          "Only approved actions");
    auto healthMode = root / "bus/wmi/drivers/acer-wmi-battery/health_mode";
    put(healthMode, "1");
    check(protection(root).path == healthMode && !protection(root).standard,
          "Installed acer-wmi-battery detected before Linuwu");
    check(read(protection(root).path) == "1",
          "Existing enabled state detected");
    put(healthMode, "0");
    check(read(protection(root).path) == "0", "Disabled state detected");
    check(permittedTarget("/sys/bus/wmi/drivers/acer-wmi-battery/health_mode",
                          true),
          "Driver attribute allowed for battery");
    check(!permittedTarget("/sys/bus/wmi/drivers/acer-wmi-battery/health_mode",
                           false),
          "Driver attribute rejected for fans");
    check(!permittedTarget(
              "/sys/bus/wmi/drivers/acer-wmi-battery/calibration_mode", true) &&
              !permittedTarget("/tmp/health_mode", true),
          "Other attributes rejected");
    put(bat / "charge_control_end_threshold", "80");
    check(protection(root).standard,
          "Standard retains priority over health mode");
    fs::remove(bat / "charge_control_end_threshold");
    put(root / "class/dmi/id/sys_vendor", "Other");
    check(protection(root).path.empty(), "Acer attribute requires Acer vendor");
    put(root / "class/dmi/id/sys_vendor", "Acer");
    auto chip = root / "class/hwmon/hwmon0";
    put(chip / "name", "coretemp");
    put(chip / "temp1_input", "48000");
    put(chip / "temp1_label", "Package");
    put(chip / "fan1_input", "0");
    put(chip / "temp2_input", "70000");
    put(chip / "temp2_fault", "1");
    auto s = sensors(root);
    check(s.size() == 3, "Sensor count");
    check(s[0].value == 0 && s[1].value == 48 && !s[2].value,
          "Sensor units and fault");
    // ---- ventoinhas, modo manual e perfis (0.3.0) ----
    check(manualPercent("fan-manual:25") == 25 &&
              manualPercent("fan-manual:100") == 100 &&
              manualPercent("fan-manual:60") == 60,
          "Manual percent accepted in range");
    check(!manualPercent("fan-manual:24") && !manualPercent("fan-manual:101") &&
              !manualPercent("fan-manual:") && !manualPercent("fan-manual:-30") &&
              !manualPercent("fan-manual:+30") &&
              !manualPercent("fan-manual: 30") &&
              !manualPercent("fan-manual:30;id") &&
              !manualPercent("fan-manual:1000") && !manualPercent("fan-manual"),
          "Manual percent rejected out of range or malformed");
    check(allowed("fan-manual:50") && !allowed("fan-manual:5") &&
              !allowed("fan-manual:50 ") && !allowed("fan-manual:0x32"),
          "Helper accepts only valid manual actions");
    check(commandValue("fan-manual:40", false) == "40,40" &&
              commandValue("fan-auto", false) == "0,0" &&
              commandValue("fan-max", false) == "100,100" &&
              commandValue("fan-manual:5", false).empty(),
          "Linuwu fan values");
    check(pwmFromPercent(25) == 64 && pwmFromPercent(100) == 255 &&
              percentFromPwm(255) == 100 && percentFromPwm(64) == 25,
          "PWM scaling");
    check(manualSpeedSafe(25, 50) && !manualSpeedSafe(24, 50) &&
              !manualSpeedSafe(101, 50),
          "Safe at low temperature, bounds enforced");
    check(!manualSpeedSafe(30, 72) && manualSpeedSafe(40, 72) &&
              !manualSpeedSafe(60, 88) && manualSpeedSafe(70, 88),
          "Minimum speed grows with temperature");
    check(!manualSpeedSafe(30, std::nullopt) &&
              manualSpeedSafe(40, std::nullopt),
          "Unknown temperature requires a higher minimum");
    check(std::abs(*cpuTemperature(s) - 48) < .01, "CPU temperature");
    check(!fanBackend(root).available() || fanBackend(root).kind ==
                                              FanKind::None,
          "No fan backend without drivers");
    auto fanSpeed = root / "module/linuwu_sense/drivers/platform:acer-wmi/"
                           "acer-wmi/nitro_sense/fan_speed";
    put(fanSpeed, "0,0");
    check(fanBackend(root).kind == FanKind::Linuwu &&
              fanBackend(root).speed == fanSpeed,
          "Linuwu fan backend detected");
    check(fanState(fanBackend(root)).known &&
              fanState(fanBackend(root)).automatic,
          "Linuwu auto state");
    put(fanSpeed, "55,55");
    check(!fanState(fanBackend(root)).automatic &&
              fanState(fanBackend(root)).percent == 55,
          "Linuwu manual state");
    put(fanSpeed, "lixo");
    check(!fanState(fanBackend(root)).known, "Linuwu garbage state unknown");
    auto acer = root / "class/hwmon/hwmon9";
    put(acer / "name", "acer");
    put(acer / "pwm1", "0");
    check(fanBackend(root).kind == FanKind::Linuwu,
          "PWM without enable file is ignored");
    put(acer / "pwm1_enable", "2");
    put(acer / "pwm2", "0");
    put(acer / "pwm2_enable", "2");
    auto fb = fanBackend(root);
    check(fb.kind == FanKind::HwmonPwm && fb.pwm.size() == 2 &&
              fb.enable.size() == 2,
          "Hwmon PWM preferred over Linuwu");
    check(fanState(fb).known && fanState(fb).automatic, "PWM auto state");
    put(acer / "pwm1_enable", "1");
    put(acer / "pwm2_enable", "1");
    put(acer / "pwm1", "128");
    check(fanState(fb).known && !fanState(fb).automatic &&
              fanState(fb).percent == 50,
          "PWM manual state and percent");
    put(acer / "pwm2_enable", "2");
    check(!fanState(fb).known, "Mixed PWM state is unknown");
    put(acer / "name", "coretemp");
    check(fanBackend(root).kind == FanKind::Linuwu,
          "Only the acer hwmon chip is used for PWM");
    put(acer / "name", "acer");
    put(root / "class/dmi/id/sys_vendor", "Other");
    check(!fanBackend(root).available() ||
              fanBackend(root).kind == FanKind::HwmonPwm,
          "Linuwu fan requires Acer vendor");
    put(root / "class/dmi/id/sys_vendor", "Acer");
    check(permittedTarget("/sys/devices/platform/acer-wmi/nitro_sense/fan_speed",
                          false),
          "fan_speed under /sys/devices allowed");
    check(!permittedTarget("/tmp/fan_speed", false) &&
              !permittedTarget("/sys/class/hwmon/hwmon9/pwm1", false) &&
              !permittedTarget("/sys/devices/x/charge_control_end_threshold",
                               false),
          "Fan targets restricted");
    check(!permittedTarget("/sys/devices/x/hwmon/hwmon9/pwm1", false),
          "PWM requires an acer hwmon parent");
    check(pwmAttribute("pwm1") && pwmAttribute("pwm2_enable") &&
              !pwmAttribute("pwm5") && !pwmAttribute("pwm1_mode") &&
              !pwmAttribute("pwm") && !pwmAttribute("fan1_input"),
          "PWM attribute names");
    check(!permittedTarget("/sys/devices/x/fan_speed", true),
          "Fan attribute rejected for battery");
    check(!platformProfile(root).present, "No platform profile");
    put(root / "class/platform-profile/platform-profile-0/name", "acer-wmi");
    put(root / "class/platform-profile/platform-profile-0/choices",
        "low-power quiet balanced");
    check(platformProfile(root).present &&
              platformProfile(root).driver == "acer-wmi" &&
              platformProfile(root).choices == "low-power quiet balanced",
          "Platform profile detected");
    // ---- aplicação hwmon: sequência, confirmação e reversão ----
    auto plain = [](const fs::path &p, const std::string &v) {
      std::ofstream(p) << v;
    };
    put(acer / "name", "acer");
    put(acer / "pwm1_enable", "2");
    put(acer / "pwm2_enable", "2");
    put(acer / "pwm1", "0");
    put(acer / "pwm2", "0");
    fb = fanBackend(root);
    applyHwmonFan(fb, "fan-manual:50", plain);
    check(read(acer / "pwm1_enable") == "1" && read(acer / "pwm2_enable") == "1" &&
              read(acer / "pwm1") == "128" && read(acer / "pwm2") == "128",
          "Manual 50% writes mode then speed on both fans");
    applyHwmonFan(fb, "fan-max", plain);
    check(read(acer / "pwm1") == "255" && read(acer / "pwm2") == "255",
          "Max writes 100%");
    applyHwmonFan(fb, "fan-auto", plain);
    check(read(acer / "pwm1_enable") == "2" && read(acer / "pwm2_enable") == "2",
          "Auto restores firmware control");
    auto failing = [&](const fs::path &p, const std::string &v) {
      if (p.filename() == "pwm2")
        throw std::runtime_error("driver recusou");
      std::ofstream(p) << v;
    };
    bool threw = false;
    try {
      applyHwmonFan(fb, "fan-manual:30", failing);
    } catch (const std::exception &) {
      threw = true;
    }
    check(threw && read(acer / "pwm1_enable") == "2" &&
              read(acer / "pwm2_enable") == "2",
          "Failed write rolls back to the previous mode");
    auto ignoring = [&](const fs::path &p, const std::string &v) {
      if (p.filename().string().rfind("pwm", 0) == 0 &&
          p.filename().string().find("_enable") == std::string::npos)
        return; // o driver aceita a escrita mas não aplica o valor
      std::ofstream(p) << v;
    };
    threw = false;
    try {
      applyHwmonFan(fb, "fan-manual:70", ignoring);
    } catch (const std::exception &e) {
      threw = std::string(e.what()).find("não confirmou") != std::string::npos;
    }
    check(threw && read(acer / "pwm1_enable") == "2",
          "Unconfirmed speed is reported and rolled back");
    threw = false;
    try {
      applyHwmonFan(fb, "fan-manual:5", plain);
    } catch (const std::exception &) {
      threw = true;
    }
    check(threw && read(acer / "pwm1_enable") == "2",
          "Invalid command changes nothing");
    // ---- perfil térmico direto (0.3.1) ----
    check(!acerProfile(root), "No writable profile without a profile file");
    auto prof = root / "class/platform-profile/platform-profile-0";
    put(prof / "choices", "low-power quiet balanced balanced-performance performance");
    put(prof / "profile", "balanced");
    check(acerProfile(root) && acerProfile(root)->profile == prof / "profile",
          "Acer platform profile found");
    put(prof / "name", "outro-driver");
    check(!acerProfile(root), "Only the acer-wmi profile is writable");
    put(prof / "name", "acer-wmi");
    check(modeFromProfile("low-power") == "eco" && modeFromProfile("quiet") == "eco" &&
              modeFromProfile("balanced") == "auto" &&
              modeFromProfile("balanced-performance") == "performance" &&
              modeFromProfile("performance") == "performance" &&
              modeFromProfile("custom").empty(),
          "Profile to mode mapping");
    check(allowed("profile-eco") && allowed("profile-auto") &&
              allowed("profile-performance") && !allowed("profile-turbo") &&
              !allowed("profile-eco;id") && !allowed("profile-"),
          "Profile actions allowed strictly");
    check(!permittedTarget("/sys/devices/x/platform-profile-0/profile", false) &&
              !permittedTarget("/tmp/platform-profile-0/profile", false) &&
              !permittedTarget("/sys/devices/x/other/profile", false),
          "Profile target restricted");
    auto profileTarget = *acerProfile(root);
    check(applyPlatformProfile(profileTarget, "eco", plain) == "low-power" &&
              read(prof / "profile") == "low-power",
          "Eco picks low-power");
    check(applyPlatformProfile(profileTarget, "auto", plain) == "balanced" &&
              read(prof / "profile") == "balanced",
          "Auto picks balanced");
    check(applyPlatformProfile(profileTarget, "performance", plain) ==
                  "performance" &&
              read(prof / "profile") == "performance",
          "Performance used when the firmware accepts it");
    put(prof / "profile", "balanced");
    auto rejectsPerformance = [](const fs::path &p, const std::string &v) {
      if (v == "performance")
        throw std::runtime_error("firmware recusou");
      std::ofstream(p) << v;
    };
    check(applyPlatformProfile(profileTarget, "performance", rejectsPerformance) ==
                  "balanced-performance" &&
              read(prof / "profile") == "balanced-performance",
          "Rejected performance falls back to balanced-performance");
    put(prof / "choices", "balanced quiet");
    profileTarget = *acerProfile(root);
    check(applyPlatformProfile(profileTarget, "eco", plain) == "quiet",
          "Eco falls back to quiet when low-power is not offered");
    put(prof / "choices", "balanced");
    profileTarget = *acerProfile(root);
    put(prof / "profile", "balanced");
    threw = false;
    try {
      applyPlatformProfile(profileTarget, "eco", plain);
    } catch (const std::exception &) {
      threw = true;
    }
    check(threw && read(prof / "profile") == "balanced",
          "No compatible profile changes nothing");
    put(prof / "choices", "low-power balanced");
    profileTarget = *acerProfile(root);
    auto ignoringProfile = [](const fs::path &, const std::string &) {};
    threw = false;
    try {
      applyPlatformProfile(profileTarget, "eco", ignoringProfile);
    } catch (const std::exception &e) {
      threw = std::string(e.what()).find("recusou") != std::string::npos;
    }
    check(threw && read(prof / "profile") == "balanced",
          "Unconfirmed profile is reported and not assumed");
    put(bat / "charge_control_end_threshold", "80");
    put(bat / "status", "Discharging");
    put(bat / "scope", "Device");
    check(batteries(root).empty(), "Peripheral batteries are not internal");
    // ---- valores absurdos vindos do driver: sem conversão indefinida ----
    check(!boundedInt(std::optional<double>(1e30), 0, 255) &&
              !boundedInt(std::optional<double>(-1), 0, 255) &&
              !boundedInt(std::nullopt, 0, 255) &&
              boundedInt(std::optional<double>(255), 0, 255) == 255 &&
              boundedInt(std::optional<double>(0), 0, 255) == 0,
          "Bounded integer conversion rejects out-of-range values");
    put(acer / "name", "acer");
    put(acer / "pwm1_enable", "1");
    put(acer / "pwm2_enable", "1");
    put(acer / "pwm1", "1e30");
    put(acer / "pwm2", "1e30");
    fb = fanBackend(root);
    check(fb.kind == FanKind::HwmonPwm && fanState(fb).known &&
              fanState(fb).percent == -1,
          "Absurd PWM value yields no percent instead of undefined behavior");
    FanBackend linuwu;
    linuwu.kind = FanKind::Linuwu;
    linuwu.speed = fanSpeed;
    put(fanSpeed, "1e30,1e30");
    check(fanState(linuwu).known && fanState(linuwu).percent == -1,
          "Absurd Linuwu value yields no percent");
    put(fanSpeed, "150,150");
    check(fanState(linuwu).known && fanState(linuwu).percent == -1,
          "Linuwu value above 100 yields no percent");
    auto absurd = [&](const fs::path &p, const std::string &v) {
      auto n = p.filename().string();
      if (n.rfind("pwm", 0) == 0 && n.find("_enable") == std::string::npos) {
        std::ofstream(p) << "99999999999999999999";
        return;
      }
      std::ofstream(p) << v;
    };
    threw = false;
    try {
      applyHwmonFan(fb, "fan-manual:70", absurd);
    } catch (const std::exception &e) {
      threw = std::string(e.what()).find("não confirmou") != std::string::npos;
    }
    check(threw && read(acer / "pwm1_enable") == "1",
          "Absurd PWM readback is never treated as confirmed");
    // ---- compatibilidade com o núcleo original (0.3.2) ----
    fs::remove_all(root / "class/power_supply");
    auto bat7 = root / "class/power_supply/BAT7";
    put(bat7 / "type", "Battery");
    put(bat7 / "present", "0");
    check(batteries(root).empty(), "Absent battery (present=0) is ignored");
    put(bat7 / "present", "1");
    check(batteries(root).size() == 1, "Present battery is listed");
    put(bat7 / "energy_full", "0");
    put(bat7 / "energy_full_design", "57400000");
    check(!health(bat7), "Zero full capacity gives no health");
    put(root / "multi", "primeira\nsegunda\n");
    check(read(root / "multi") == "primeira", "read() returns only the first line");
    check(number("5 ") == std::nullopt, "Number with trailing space rejected");
    fs::remove_all(root);
    std::cout << "All " << checks << " core checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    fs::remove_all(root);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
