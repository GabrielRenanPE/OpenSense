#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>
namespace osense {
namespace fs = std::filesystem;
inline std::string read(const fs::path &p) {
  std::ifstream f(p);
  std::string s;
  std::getline(f, s);
  return s;
}
inline std::optional<double> number(const std::string &s) {
  try {
    size_t n;
    double d = std::stod(s, &n);
    if (n == s.size() && std::isfinite(d))
      return d;
  } catch (...) {
  }
  return {};
}
// Converte para int somente se o valor estiver dentro de [lo, hi]. Valores
// lidos do sysfs vêm de drivers; um double fora do alcance de int tem
// comportamento indefinido ao ser convertido, então nunca convertemos às cegas.
inline std::optional<int> boundedInt(const std::optional<double> &v, int lo,
                                     int hi) {
  if (v && *v >= lo && *v <= hi)
    return int(*v);
  return std::nullopt;
}
inline std::vector<fs::path> children(const fs::path &p) {
  std::vector<fs::path> v;
  std::error_code e;
  for (fs::directory_iterator i(p, e), end; i != end && !e; i.increment(e))
    v.push_back(i->path());
  std::sort(v.begin(), v.end());
  return v;
}
inline bool exists(const fs::path &p) {
  std::error_code e;
  return fs::exists(p, e);
}
inline fs::path sense(const std::string &name, const fs::path &sys = "/sys") {
  for (auto group : {"nitro_sense", "predator_sense"}) {
    auto p = sys / "module/linuwu_sense/drivers/platform:acer-wmi/acer-wmi" /
             group / name;
    if (osense::exists(p))
      return p;
  }
  return {};
}
inline std::vector<fs::path> batteries(const fs::path &sys = "/sys") {
  std::vector<fs::path> v;
  for (auto p : children(sys / "class/power_supply"))
    if (read(p / "type") == "Battery" && read(p / "scope") != "Device" &&
        read(p / "present") != "0")
      v.push_back(p);
  return v;
}
struct Health {
  double full, design, percent;
  std::string unit;
};
inline std::optional<Health> health(const fs::path &p) {
  for (auto prefix : {"energy", "charge"}) {
    auto f = number(read(p / (std::string(prefix) + "_full"))),
         d = number(read(p / (std::string(prefix) + "_full_design")));
    if (f && d && *f > 0 && *d > 0)
      return Health{*f / 1e6, *d / 1e6, 100 * *f / *d,
                    std::string(prefix) == "energy" ? "Wh" : "Ah"};
  }
  return {};
}
struct Control {
  fs::path path;
  bool standard = false;
};
inline Control protection(const fs::path &sys = "/sys") {
  auto b = batteries(sys);
  if (b.size() != 1)
    return {};
  auto p = b[0] / "charge_control_end_threshold";
  if (osense::exists(p))
    return {p, true};
  auto vendor = read(sys / "class/dmi/id/sys_vendor");
  std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
  if (vendor.find("acer") != std::string::npos) {
    auto healthMode = sys / "bus/wmi/drivers/acer-wmi-battery/health_mode";
    if (osense::exists(healthMode))
      return {healthMode, false};
    return {sense("battery_limiter", sys), false};
  }
  return {};
}
// ---------------------------------------------------------------- 0.3.x
// Tudo acima é o núcleo original, sem alterações. O que vem abaixo é novo:
// ventoinhas, modo manual e perfil térmico. Estes helpers não mudam o
// comportamento de read()/number() usados pelo código antigo.
inline constexpr int kFanMin = 25;  // limite inferior do modo manual (%)
inline constexpr int kFanMax = 100; // limite superior do modo manual (%)
// Valores de pwmN_enable da interface hwmon padrão do kernel.
inline constexpr const char *kPwmManual = "1";
inline constexpr const char *kPwmAuto = "2";

inline std::string trim(std::string s) {
  auto space = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && space(s.back()))
    s.pop_back();
  size_t i = 0;
  while (i < s.size() && space(s.at(i)))
    ++i;
  return s.substr(i);
}
inline std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}
// Primeira linha do arquivo, sem espaços nas pontas (usado só pelo código novo).
inline std::string value(const fs::path &p) { return trim(read(p)); }
inline bool pathExists(const fs::path &p) { return osense::exists(p); }
inline bool isAcer(const fs::path &sys = "/sys") {
  return lower(read(sys / "class/dmi/id/sys_vendor")).find("acer") !=
         std::string::npos;
}
// Diretórios filhos de `dir` cujo nome é <prefix><número>, em ordem numérica.
inline std::vector<fs::path> numberedDirs(const fs::path &dir,
                                          const std::string &prefix) {
  std::vector<std::pair<int, fs::path>> found;
  for (auto &p : children(dir)) {
    auto name = p.filename().string();
    if (name.rfind(prefix, 0) != 0 || name.size() == prefix.size() ||
        name.size() > prefix.size() + 6)
      continue;
    auto digits = name.substr(prefix.size());
    if (std::all_of(digits.begin(), digits.end(),
                    [](unsigned char c) { return std::isdigit(c) != 0; }))
      found.emplace_back(std::stoi(digits), p);
  }
  std::sort(found.begin(), found.end());
  std::vector<fs::path> out;
  for (auto &f : found)
    out.push_back(f.second);
  return out;
}

struct Sensor {
  std::string chip, label, unit;
  std::optional<double> value;
};
inline std::vector<Sensor> sensors(const fs::path &sys = "/sys") {
  std::vector<Sensor> result;
  for (auto chip : children(sys / "class/hwmon")) {
    auto name = read(chip / "name");
    for (auto p : children(chip)) {
      auto n = p.filename().string();
      bool temp = n.rfind("temp", 0) == 0, fan = n.rfind("fan", 0) == 0;
      if (!(temp || fan) || n.size() < 7 || n.substr(n.size() - 6) != "_input")
        continue;
      auto base = n.substr(0, n.size() - 6);
      auto label = read(chip / (base + "_label"));
      auto v = number(read(p));
      if (read(chip / (base + "_fault")) == "1")
        v = {};
      if (v && temp)
        *v /= 1000;
      result.push_back(
          {name, label.empty() ? base : label, temp ? "°C" : "RPM", v});
    }
  }
  return result;
}

// Maior temperatura entre os sensores de CPU conhecidos.
inline std::optional<double> cpuTemperature(const std::vector<Sensor> &all) {
  std::optional<double> best;
  for (auto &s : all)
    if (s.unit == "°C" && s.value &&
        (s.chip == "coretemp" || s.chip == "k10temp" || s.chip == "zenpower"))
      best = best ? std::max(*best, *s.value) : *s.value;
  return best;
}

// --------------------------------------------------------------- ventoinhas
// Prioridade: PWM do acer-wmi (hwmon "acer"); depois fan_speed do Linuwu-Sense.
enum class FanKind { None, HwmonPwm, Linuwu };
struct FanBackend {
  FanKind kind = FanKind::None;
  std::vector<fs::path> pwm, enable; // pares pwmN / pwmN_enable
  fs::path speed;                    // Linuwu: fan_speed ("cpu,gpu" em %)
  bool available() const { return kind != FanKind::None; }
};
inline FanBackend fanBackend(const fs::path &root = "/sys") {
  FanBackend fb;
  for (auto dir : numberedDirs(root / "class/hwmon", "hwmon")) {
    if (value(dir / "name") != "acer")
      continue;
    for (int n = 1; n <= 4; ++n) {
      auto pwm = dir / ("pwm" + std::to_string(n));
      auto enable = dir / ("pwm" + std::to_string(n) + "_enable");
      if (pathExists(pwm) && pathExists(enable)) {
        fb.pwm.push_back(pwm);
        fb.enable.push_back(enable);
      }
    }
    if (!fb.pwm.empty()) {
      fb.kind = FanKind::HwmonPwm;
      return fb;
    }
  }
  auto speed = isAcer(root) ? sense("fan_speed", root) : fs::path{};
  if (!speed.empty()) {
    fb.kind = FanKind::Linuwu;
    fb.speed = speed;
  }
  return fb;
}
inline int pwmFromPercent(int percent) { return (percent * 255 + 50) / 100; }
inline int percentFromPwm(int pwm) { return (pwm * 100 + 127) / 255; }

struct FanState {
  bool known = false;
  bool automatic = false;
  int percent = -1; // somente em modo manual
};
inline FanState fanState(const FanBackend &fb) {
  FanState st;
  if (fb.kind == FanKind::HwmonPwm && !fb.enable.empty()) {
    bool allAuto = true, allManual = true;
    for (auto &e : fb.enable) {
      auto v = value(e);
      allAuto = allAuto && v == kPwmAuto;
      allManual = allManual && v == kPwmManual;
    }
    if (allAuto) {
      st.known = st.automatic = true;
    } else if (allManual) {
      st.known = true;
      if (auto n = boundedInt(number(value(fb.pwm[0])), 0, 255))
        st.percent = percentFromPwm(*n);
    }
  } else if (fb.kind == FanKind::Linuwu) {
    auto v = value(fb.speed);
    auto comma = v.find(',');
    if (comma == std::string::npos)
      return st;
    auto a = number(v.substr(0, comma)), b = number(v.substr(comma + 1));
    if (!a || !b)
      return st;
    st.known = true;
    if (*a == 0 && *b == 0)
      st.automatic = true;
    else if (*a == *b) {
      if (auto pct = boundedInt(a, 0, 100))
        st.percent = *pct;
    }
  }
  return st;
}

// "fan-manual:60" → 60, somente inteiros de 25 a 100 sem sinal nem espaços.
inline std::optional<int> manualPercent(const std::string &cmd) {
  const std::string prefix = "fan-manual:";
  if (cmd.rfind(prefix, 0) != 0)
    return std::nullopt;
  auto digits = cmd.substr(prefix.size());
  if (digits.empty() || digits.size() > 3 ||
      !std::all_of(digits.begin(), digits.end(),
                   [](unsigned char c) { return std::isdigit(c) != 0; }))
    return std::nullopt;
  int value = std::stoi(digits);
  if (value < kFanMin || value > kFanMax)
    return std::nullopt;
  return value;
}
// Proteção conservadora do modo manual; não substitui a proteção do firmware.
inline bool manualSpeedSafe(int percent, std::optional<double> cpuTemp) {
  if (percent < kFanMin || percent > kFanMax)
    return false;
  if (!cpuTemp)
    return percent >= 40;
  if (*cpuTemp >= 85)
    return percent >= 70;
  if (*cpuTemp >= 70)
    return percent >= 40;
  return true;
}


// Aplica auto/máximo/manual na interface hwmon (pwmN / pwmN_enable). O escritor
// é injetado: o auxiliar usa um que revalida o caminho; os testes usam arquivos.
// Se algo falhar depois de entrar em manual, devolve o modo anterior para não
// deixar a ventoinha presa em uma velocidade desconhecida.
using WriteFn = std::function<void(const fs::path &, const std::string &)>;
inline void applyHwmonFan(const FanBackend &fb, const std::string &cmd,
                          const WriteFn &write) {
  std::vector<std::string> before;
  for (auto &e : fb.enable)
    before.push_back(value(e));
  try {
    if (cmd == "fan-auto") {
      for (auto &e : fb.enable)
        write(e, kPwmAuto);
      for (auto &e : fb.enable)
        if (value(e) != kPwmAuto)
          throw std::runtime_error("O driver não confirmou o modo automático.");
      return;
    }
    int percent = cmd == "fan-max" ? kFanMax : -1;
    if (auto p = manualPercent(cmd))
      percent = *p;
    if (percent < kFanMin)
      throw std::runtime_error("Comando de ventoinha inválido.");
    int target = pwmFromPercent(percent);
    for (auto &e : fb.enable)
      write(e, kPwmManual);
    for (auto &p : fb.pwm)
      write(p, std::to_string(target));
    for (size_t i = 0; i < fb.pwm.size(); ++i) {
      auto now = boundedInt(number(value(fb.pwm[i])), 0, 255);
      if (value(fb.enable[i]) != kPwmManual || !now ||
          std::abs(*now - target) > 3)
        throw std::runtime_error(
            "A escrita foi enviada, mas o driver não confirmou a velocidade "
            "solicitada.");
    }
  } catch (...) {
    for (size_t i = 0; i < fb.enable.size(); ++i)
      if (!before[i].empty())
        try {
          write(fb.enable[i], before[i]);
        } catch (...) {
        }
    throw;
  }
}

// ------------------------------------------------------- perfil de plataforma
struct PlatformProfile {
  bool present = false;
  std::string driver, choices;
};
inline PlatformProfile platformProfile(const fs::path &root = "/sys") {
  PlatformProfile info;
  for (auto dir : numberedDirs(root / "class/platform-profile",
                               "platform-profile-")) {
    info.present = true;
    info.driver = value(dir / "name");
    info.choices = value(dir / "choices");
    return info;
  }
  if (pathExists(root / "firmware/acpi/platform_profile")) {
    info.present = true;
    info.driver = "acpi";
    info.choices = value(root / "firmware/acpi/platform_profile_choices");
  }
  return info;
}


// Perfil térmico gravável: somente o dispositivo "acer-wmi" do platform_profile.
struct ProfileTarget {
  fs::path profile;
  std::string choices;
};
inline std::optional<ProfileTarget> acerProfile(const fs::path &root = "/sys") {
  for (auto dir :
       numberedDirs(root / "class/platform-profile", "platform-profile-")) {
    auto profile = dir / "profile";
    if (value(dir / "name") == "acer-wmi" && pathExists(profile))
      return ProfileTarget{profile, value(dir / "choices")};
  }
  return std::nullopt;
}
// Perfis candidatos por modo, do preferido ao alternativo. O firmware do
// ANV15-51 recusa "performance"; então "balanced-performance" é o mais alto.
inline std::vector<std::string> profileCandidates(const std::string &mode) {
  if (mode == "eco")
    return {"low-power", "quiet"};
  if (mode == "auto")
    return {"balanced"};
  if (mode == "performance")
    return {"performance", "balanced-performance"};
  return {};
}
inline std::string modeFromProfile(const std::string &value) {
  if (value == "low-power" || value == "quiet")
    return "eco";
  if (value == "balanced")
    return "auto";
  if (value == "balanced-performance" || value == "performance")
    return "performance";
  return {};
}
// Aplica o primeiro candidato aceito pelo firmware e confirmado por leitura.
// Devolve o perfil aplicado; se nenhum servir, restaura o anterior e lança.
inline std::string applyPlatformProfile(const ProfileTarget &target,
                                        const std::string &mode,
                                        const WriteFn &write) {
  std::istringstream tokens(target.choices);
  std::vector<std::string> offered;
  for (std::string t; tokens >> t;)
    offered.push_back(t);
  auto previous = value(target.profile);
  std::string tried;
  for (auto &candidate : profileCandidates(mode)) {
    if (std::find(offered.begin(), offered.end(), candidate) == offered.end())
      continue;
    tried += (tried.empty() ? "" : ", ") + candidate;
    try {
      write(target.profile, candidate);
    } catch (...) {
      continue;
    }
    if (value(target.profile) == candidate)
      return candidate;
  }
  if (!previous.empty() && value(target.profile) != previous)
    try {
      write(target.profile, previous);
    } catch (...) {
    }
  throw std::runtime_error(
      tried.empty()
          ? "O firmware não oferece nenhum perfil compatível com este modo."
          : "O firmware recusou os perfis tentados (" + tried +
                "). Nenhuma alteração confirmada.");
}

// ----------------------------------------------------------- ações do auxiliar
inline bool allowed(const std::string &cmd) {
  for (auto fixed : {"gpu-hybrid", "gpu-integrated", "battery-on",
                     "battery-off", "fan-auto", "fan-max", "profile-eco",
                     "profile-auto", "profile-performance"})
    if (cmd == fixed)
      return true;
  return manualPercent(cmd).has_value();
}
inline std::string commandValue(const std::string &cmd, bool standard) {
  if (cmd == "battery-on")
    return standard ? "80" : "1";
  if (cmd == "battery-off")
    return standard ? "100" : "0";
  if (cmd == "fan-auto")
    return "0,0";
  if (cmd == "fan-max")
    return "100,100";
  if (auto p = manualPercent(cmd))
    return std::to_string(*p) + "," + std::to_string(*p);
  return {};
}
inline bool pwmAttribute(const std::string &name) {
  if (name.size() < 4 || name.rfind("pwm", 0) != 0 || name[3] < '1' ||
      name[3] > '4')
    return false;
  return name.size() == 4 || name.substr(4) == "_enable";
}
// Driver-wide health_mode stays under /sys/bus; allow only this exact
// attribute. (0.3.x: além disso, só atributos conhecidos em /sys/devices.)
// `path` deve ser o caminho canônico.
inline bool permittedTarget(const fs::path &path, bool battery) {
  auto s = path.generic_string();
  auto name = path.filename().string();
  if (battery) {
    if (s == "/sys/bus/wmi/drivers/acer-wmi-battery/health_mode")
      return true;
    return s.rfind("/sys/devices/", 0) == 0 &&
           (name == "charge_control_end_threshold" || name == "battery_limiter");
  }
  if (s.rfind("/sys/devices/", 0) != 0)
    return false;
  if (name == "fan_speed")
    return true;
  if (name == "profile")
    return path.parent_path().filename().string().rfind("platform-profile-", 0) ==
               0 &&
           value(path.parent_path() / "name") == "acer-wmi";
  return pwmAttribute(name) && value(path.parent_path() / "name") == "acer";
}
} // namespace osense
