// Konfigurasi notifikasi Telegram (Bahasa Indonesia)
// Isi TELEGRAM_BOT_TOKEN dan TELEGRAM_CHAT_ID dengan data bot Anda.

#ifndef TELEGRAM_CONFIG_H
#define TELEGRAM_CONFIG_H

// Aktifkan/Non-aktifkan notifikasi Telegram
#define TELEGRAM_ENABLED 1

// Token bot Telegram: ganti dengan token Anda (contoh: "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11")
#define TELEGRAM_BOT_TOKEN "8979405099:AAG6Yoza-gOTIyjOX9o04BAVA7m036tjupI"

// Chat ID tujuan: ganti dengan chat id atau user id yang akan menerima notifikasi
#define TELEGRAM_CHAT_ID "1218941164"

// Atur apakah ingin menerima notifikasi untuk level WARNING & CRITICAL
#define TELEGRAM_NOTIFY_WARNING 1
#define TELEGRAM_NOTIFY_CRITICAL 1

#endif // TELEGRAM_CONFIG_H
