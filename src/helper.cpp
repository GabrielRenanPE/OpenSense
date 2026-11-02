#include "system.hpp"
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace osense;

// Grava em um atributo permitido; o caminho canônico é revalidado a cada vez.
static void writeAttribute(const fs::path &target, const std::string &value,
                           bool battery) {
  auto path = fs::canonical(target);
  if (!permittedTarget(path, battery))
    throw std::runtime_error("Interface de hardware inesperada.");
  std::ofstream f(path);
  if (!f)
    throw std::runtime_error("Driver não permitiu a escrita.");
  f << value << '\n';
  f.flush();
  if (!f)
    throw std::runtime_error("Driver recusou a alteração.");
}

int main(int argc, char **argv) {
  if (argc != 2 || !allowed(argv[1])) {
    std::cerr << "Operação não permitida.\n";
    return 2;
  }
#ifdef OPENSENSE_FAN_ONLY
  // This binary has a separate Polkit action. Never permit GPU/battery here.
  const std::string operation = argv[1];
  if (!(operation.rfind("fan-", 0) == 0 ||
        operation.rfind("profile-", 0) == 0)) {
    std::cerr << "Operação fora do controle de resfriamento.\n";
    return 2;
  }
#endif
  if (geteuid() != 0) {
    std::cerr << "Autenticação administrativa necessária.\n";
    return 2;
  }
  int lock = open("/run/opensense.lock", O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
  if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {
    std::cerr << "Outra alteração está em andamento.\n";
    return 2;
  }
  std::string cmd = argv[1];
  try {
#ifndef OPENSENSE_FAN_ONLY
    if (cmd.rfind("gpu-", 0) == 0) {
      struct stat st{};
      if (stat("/usr/bin/envycontrol", &st) || st.st_uid != 0 ||
          (st.st_mode & 0022) || !S_ISREG(st.st_mode))
        throw std::runtime_error(
            "Instale o EnvyControl pelo gerenciador de pacotes. Executável "
            "confiável não encontrado em /usr/bin/envycontrol.");
      char program[] = "/usr/bin/envycontrol", sw[] = "-s", hybrid[] = "hybrid",
           integrated[] = "integrated";
      char *args[] = {program, sw, cmd == "gpu-hybrid" ? hybrid : integrated,
                      nullptr};
      char path[] = "PATH=/usr/sbin:/usr/bin:/sbin:/bin", home[] = "HOME=/root",
           lang[] = "LC_ALL=C";
      char *env[] = {path, home, lang, nullptr};
      execve(program, args, env);
      throw std::runtime_error("Não foi possível iniciar o EnvyControl.");
    }
#endif
    if (cmd.rfind("profile-", 0) == 0) {
      if (!isAcer())
        throw std::runtime_error(
            "Perfil térmico requer interface Acer compatível.");
      auto target = acerProfile();
      if (!target)
        throw std::runtime_error("Perfil de plataforma do acer-wmi não "
                                 "disponível. Nenhuma alteração realizada.");
      auto applied = applyPlatformProfile(
          *target, cmd.substr(8), [](const fs::path &p, const std::string &v) {
            writeAttribute(p, v, false);
          });
      std::cout << "Perfil térmico confirmado pelo firmware: " << applied
                << ".\n";
      return 0;
    }
    if (cmd.rfind("fan-", 0) == 0) {
      if (!isAcer())
        throw std::runtime_error(
            "Controle de ventoinha requer interface Acer compatível.");
      auto fb = fanBackend();
      if (!fb.available())
        throw std::runtime_error("Controle não disponibilizado pelo driver. "
                                 "Nenhuma alteração realizada.");
      if (auto percent = manualPercent(cmd))
        if (!manualSpeedSafe(*percent, cpuTemperature(sensors())))
          throw std::runtime_error(
              "Velocidade recusada por segurança: com a temperatura atual (ou "
              "sem leitura dela) o mínimo permitido é maior. Aumente a "
              "velocidade ou use o modo Automático.");
      if (fb.kind == FanKind::HwmonPwm) {
        applyHwmonFan(fb, cmd, [](const fs::path &p, const std::string &v) {
          writeAttribute(p, v, false);
        });
        std::cout << "Modo da ventoinha confirmado pelo driver. Confira as RPM "
                     "na Visão geral.\n";
        return 0;
      }
      writeAttribute(fb.speed, commandValue(cmd, false), false);
      std::cout << "Comando enviado. Confira a rotação informada pelos "
                   "sensores.\n";
      return 0;
    }
#ifndef OPENSENSE_FAN_ONLY
    Control c = protection();
    if (c.path.empty())
      throw std::runtime_error("Controle não disponibilizado pelo driver. "
                               "Nenhuma alteração realizada.");
    if (c.standard && cmd == "battery-on") {
      auto start =
          number(read(c.path.parent_path() / "charge_control_start_threshold"));
      if (start && *start >= 80)
        throw std::runtime_error(
            "O limite de início de carga já é 80% ou maior. Ajuste o "
            "gerenciador de bateria existente antes de ativar a proteção.");
    }
    auto value = commandValue(cmd, c.standard);
    writeAttribute(c.path, value, true);
    if (read(fs::canonical(c.path)) != value)
      throw std::runtime_error(
          "A escrita foi enviada, mas o driver não confirmou o valor "
          "solicitado. Consulte o estado exibido; a configuração pode ter "
          "mudado parcialmente.");
    std::cout << "Proteção confirmada pelo driver.\n";
    return 0;
#else
    // O binário de resfriamento não contém código de GPU nem de bateria.
    throw std::runtime_error("Operação fora do controle de resfriamento.");
#endif
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
