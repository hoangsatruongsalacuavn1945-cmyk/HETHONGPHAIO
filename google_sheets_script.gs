/**
 * ============================================================================
 * GOOGLE APPS SCRIPT - HỆ THỐNG PHAO QUAN TRẮC CHEMSTATION AI v4.3-DISCORD
 * (TỰ ĐỘNG LƯU GOOGLE SHEETS & BẮN THÔNG BÁO DISCORD WEBHOOK + MAP 3D SIÊU CHI TIẾT)
 * ============================================================================
 * HƯỚNG DẪN CÀI ĐẶT TRONG 1 PHÚT:
 * 1. Mở file Google Sheets trên Google Drive của bạn.
 * 2. Vào menu: Tiện ích mở rộng (Extensions) -> Apps Script.
 * 3. Xóa code cũ, dán toàn bộ file này vào và bấm biểu tượng Lưu (Save / Ctrl+S).
 * 4. Bấm "Triển khai" (Deploy) ở góc trên bên phải -> "Tạo mục triển khai mới" (New deployment).
 * 5. Chọn loại: "Ứng dụng web" (Web app):
 *    - Thực thi dưới dạng (Execute as): "Tôi" (Me)
 *    - Ai có quyền truy cập (Who has access): "Bất kỳ ai" (Anyone)  <-- BẮT BUỘC!
 * 6. Bấm "Triển khai" (Deploy), cấp quyền truy cập (Authorize), sau đó copy "URL ứng dụng web"
 *    (có dạng: https://script.google.com/macros/s/AKfycb.../exec).
 * 7. Dán URL đó vào giao diện Web Trạm bờ (http://192.168.4.1) hoặc cài trong firmware.
 *
 * TÍCH HỢP DISCORD WEBHOOK:
 *   - Cách 1 (Dễ nhất): Tạo Webhook trong kênh Discord (Server Settings -> Integrations -> Webhooks),
 *     copy Webhook URL rồi dán vào CONFIG.DISCORD_WEBHOOK_URL ở dòng 35 bên dưới.
 *   - Cách 2: Vào Apps Script -> Project Settings (Cài đặt dự án) -> Script Properties:
 *     Thêm thuộc tính DISCORD_WEBHOOK_URL = [URL Webhook của bạn].
 *   - Cách 3: Nhập trực tiếp trên Web Dashboard Trạm bờ (http://192.168.4.1), ESP32 sẽ tự
 *     gửi thẳng qua Wi-Fi và đồng bộ!
 * ============================================================================
 */

var CONFIG = {
  // Để trống ("") nếu script được tạo trực tiếp từ Google Sheets (bound script).
  SPREADSHEET_ID: "188ibulljtyfxsrM7BwbESRmD0rsrUSVqY60F3cL4FII",
  SHEET_TELE: "Telemetry_Realtime",
  SHEET_PROFILE: "Profile_3Tang",
  ALARM_EMAIL: "",            // Ví dụ: "ban@gmail.com" (hoặc đặt trong Script properties)
  ALARM_COOLDOWN_SEC: 900,    // Giãn cách gửi email cảnh báo (15 phút)
  DEDUPE_WINDOW_SEC: 5,       // Chống ghi trùng gói tin trong 5 giây

  // DISCORD WEBHOOK INTEGRATION
  DISCORD_WEBHOOK_URL: "https://discord.com/api/webhooks/YOUR_WEBHOOK_ID/YOUR_WEBHOOK_TOKEN", // Cài đặt bảo mật qua Project Settings > Script Properties > DISCORD_WEBHOOK_URL
  DISCORD_PERIODIC_SEC: 60,   // Giãn cách gửi báo cáo định kỳ về Discord (mặc định 60 giây / 1 tin)
  DISCORD_NOTIFY_EVERY_TELEMETRY: false, // true: gửi mọi gói telemetry; false: gửi định kỳ 60s + khi có cảnh báo/profile
  GCS_WEB_URL: "https://usv-aqua-sentinel.web.app",     // Địa chỉ Web Dashboard Trạm bờ GCS (AP Mode hoặc MDNS)

  // B-01 & B-02: Bảo mật API Token (Tuỳ chọn) & Giới hạn chống tràn Quota Google Apps Script
  API_SECRET_TOKEN: "",       // Nếu đặt trong Script Properties (API_SECRET_TOKEN), mọi request phải kèm ?token=...
  MAX_BATCH_SIZE: 50,         // Giới hạn tối đa 50 bản ghi mỗi lần gọi mode=batch
  MAX_DAILY_WEBHOOK_CALLS: 1500, // Giới hạn số lần gọi UrlFetchApp.fetch mỗi ngày tránh vượt Quota 20,000/ngày

  SCRIPT_VERSION: "4.3-DISCORD"
};

var TELE_HEADERS = [
  "Thời gian",
  "Mã gói (#Seq)",
  "Độ pH",
  "Oxy hòa tan DO (mg/L)",
  "Tổng khoáng TDS (ppm)",
  "EC (µS/cm)",
  "Độ đục (NTU)",
  "Nhiệt độ nước (°C)",
  "Độ sâu tời (m)",
  "Công suất (W)",
  "Dòng điện (A)",
  "Điện áp Pin (V)",
  "Góc Roll (°)",
  "Góc Pitch (°)",
  "Hướng La bàn (°)",
  "Vĩ độ GPS (Lat)",
  "Kinh độ GPS (Lng)",
  "Vệ tinh GPS",
  "Tốc độ GPS (km/h)",
  "Độ cao GPS (m)",
  "Cách Home (m)",
  "Hướng mục tiêu (°)",
  "Cờ trạng thái 2",
  "Sóng LoRa (dBm)",
  "Sức khỏe HW (%)",
  "Chỉ số WQI",
  "Cảnh báo",
  "Đánh giá QCVN",
  "Nguồn truyền",
  "Phiên bản FW"
];

var PROFILE_HEADERS = [
  "Thời gian",
  "pH Mặt", "TDS Mặt (ppm)", "NTU Mặt", "Nhiệt Mặt (°C)", "DO Mặt (mg/L)",
  "pH Giữa", "TDS Giữa (ppm)", "NTU Giữa", "Nhiệt Giữa (°C)", "DO Giữa (mg/L)",
  "pH Đáy", "TDS Đáy (ppm)", "NTU Đáy", "Nhiệt Đáy (°C)", "DO Đáy (mg/L)",
  "Nguồn gửi", "Phiên bản FW"
];

// Định dạng hiển thị chuẩn xác cho từng cột của bảng Telemetry_Realtime (30 cột)
var TELE_COL_FORMATS = [
  "yyyy-mm-dd hh:mm:ss", // 1. Thời gian
  "0",                   // 2. Mã gói (#Seq) - Số nguyên
  "0.00",                // 3. Độ pH
  "0.00",                // 4. Oxy hòa tan DO (mg/L)
  "0",                   // 5. Tổng khoáng TDS (ppm) - Số nguyên
  "0",                   // 6. EC (µS/cm) - Số nguyên
  "0",                   // 7. Độ đục (NTU) - Số nguyên
  "0.0",                 // 8. Nhiệt độ nước (°C)
  "0.00",                // 9. Độ sâu tời (m)
  "0.00",                // 10. Công suất (W)
  "0.00",                // 11. Dòng điện (A)
  "0.00",                // 12. Điện áp Pin (V)
  "0.0",                 // 13. Góc Roll (°)
  "0.0",                 // 14. Góc Pitch (°)
  "0",                   // 15. Hướng La bàn (°)
  "0.00000",             // 16. Vĩ độ GPS (Lat)
  "0.00000",             // 17. Kinh độ GPS (Lng)
  "0",                   // 18. Vệ tinh GPS - Số nguyên
  "0.00",                // 19. Tốc độ GPS (km/h)
  "0.0",                 // 20. Độ cao GPS (m)
  "0.0",                 // 21. Cách Home (m)
  "0",                   // 22. Hướng mục tiêu (°)
  "0",                   // 23. Cờ trạng thái 2 (Flags2) - Số nguyên
  "0",                   // 24. Sóng LoRa (dBm) - Số nguyên
  "0",                   // 25. Sức khỏe HW (%) - Số nguyên
  "0",                   // 26. Chỉ số WQI - Số nguyên
  "@",                   // 27. Cảnh báo - Chuỗi văn bản
  "@",                   // 28. Đánh giá QCVN - Chuỗi văn bản
  "@",                   // 29. Nguồn truyền - Chuỗi văn bản
  "@"                    // 30. Phiên bản FW - Chuỗi văn bản
];

// Định dạng hiển thị chuẩn xác cho từng cột của bảng Profile_3Tang (18 cột)
var PROFILE_COL_FORMATS = [
  "yyyy-mm-dd hh:mm:ss",           // 1. Thời gian
  "0.00", "0", "0", "0.0", "0.00", // 2..6: Tầng Mặt (pH, TDS, NTU, Nhiệt, DO)
  "0.00", "0", "0", "0.0", "0.00", // 7..11: Tầng Giữa (pH, TDS, NTU, Nhiệt, DO)
  "0.00", "0", "0", "0.0", "0.00", // 12..16: Tầng Đáy (pH, TDS, NTU, Nhiệt, DO)
  "@", "@"                         // 17..18: Nguồn gửi, Phiên bản FW - Chuỗi văn bản
];

// ============================================================================
// ĐIỂM VÀO
// ============================================================================
function doGet(e) {
  return handleRequest(e);
}

function doPost(e) {
  return handleRequest(e);
}

function handleRequest(e) {
  var p = parseParams_(e);
  var mode = String(p.mode || "").toLowerCase();

  // 1. CÁC THAO TÁC ĐỌC (READ-ONLY) — PHẢN HỒI NGAY LẬP TỨC, KHÔNG BAO GIỜ KHÓA/CHẶN CÁC LỆNH KHÁC
  if ((!p.mode && (!e || !e.parameter || Object.keys(e.parameter).length === 0)) || mode === "web") {
    var ss = null;
    try { ss = getSpreadsheet_(); } catch (ignore) {}
    return renderDashboardHtml_(ss);
  }

  if (mode === "ping") return textOut_("PONG v" + CONFIG.SCRIPT_VERSION);

  if (mode === "status") {
    var ss = getSpreadsheet_();
    return statusOut_(ss);
  }

  if (mode === "discord_test") {
    var ok = testDiscordWebhook();
    return jsonOut_({
      ok: ok,
      message: ok ? "Đã bắn thông báo Rich Embed thành công vào kênh Discord!" : "Chưa cấu hình hoặc URL Webhook không hợp lệ."
    });
  }

  // 2. CÁC THAO TÁC GHI VÀO SỔ TÍCH CỰC (WRITE) — DÙNG KHÓA ĐỂ BẢO VỆ DỮ LIỆU
  var lock = LockService.getScriptLock();
  if (!lock.tryLock(15000)) {
    return textOut_("BUSY: he thong dang ghi du lieu, thu lai sau.");
  }

  try {
    // B-01: Xác thực API_SECRET_TOKEN nếu có
    var expectedToken = String(props_().getProperty("API_SECRET_TOKEN") || CONFIG.API_SECRET_TOKEN || "").trim();
    if (expectedToken.length > 0 && String(p.token || "").trim() !== expectedToken) {
      return textOut_("ERR_UNAUTHORIZED");
    }

    var ss = getSpreadsheet_();

    if (mode === "batch" || Array.isArray(p.items) || Array.isArray(p.batch)) {
      var batchList = Array.isArray(p.batch) ? p.batch : (Array.isArray(p.items) ? p.items : []);
      var maxBatch = Number(CONFIG.MAX_BATCH_SIZE || 50);
      if (batchList.length > maxBatch) {
        batchList = batchList.slice(0, maxBatch);
      }
      var count = writeTelemetryBatch_(ss, batchList);
      return textOut_("OK_BATCH_" + count);
    }

    if (mode === "profile") {
      writeProfile_(ss, p);
      return textOut_("OK_PROFILE");
    }

    // Mặc định: telemetry
    writeTelemetry_(ss, p);
    return textOut_("OK_TELEMETRY");
  } catch (err) {
    return textOut_("ERR:" + (err && err.message ? err.message : err));
  } finally {
    try { lock.releaseLock(); } catch (ignore) {}
  }
}

// ============================================================================
// TIỆN ÍCH
// ============================================================================
function textOut_(s) {
  return ContentService.createTextOutput(s).setMimeType(ContentService.MimeType.TEXT);
}

function jsonOut_(obj) {
  return ContentService.createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}

function props_() {
  return PropertiesService.getScriptProperties();
}

function getSpreadsheet_() {
  var ss = null;
  try { ss = SpreadsheetApp.getActiveSpreadsheet(); } catch (ignore) {}
  if (ss) return ss;

  var id = props_().getProperty("SPREADSHEET_ID") || CONFIG.SPREADSHEET_ID;
  if (!id) {
    throw new Error("Chua cau hinh SPREADSHEET_ID trong Script properties hoac CONFIG.");
  }
  return SpreadsheetApp.openById(id);
}

/** Gộp tham số GET và body JSON/POST. */
function parseParams_(e) {
  var out = {};
  var p = (e && e.parameter) ? e.parameter : {};
  for (var k in p) {
    if (Object.prototype.hasOwnProperty.call(p, k)) out[k] = p[k];
  }
  try {
    if (e && e.postData && e.postData.contents) {
      var body = e.postData.contents;
      var parsed = null;
      try { parsed = JSON.parse(body); } catch (ignore) {}
      if (parsed && typeof parsed === "object") {
        for (var j in parsed) {
          if (Object.prototype.hasOwnProperty.call(parsed, j)) out[j] = parsed[j];
        }
      } else {
        // application/x-www-form-urlencoded
        body.split("&").forEach(function (kv) {
          var i = kv.indexOf("=");
          if (i > 0) {
            out[decodeURIComponent(kv.substring(0, i))] =
              decodeURIComponent(kv.substring(i + 1).replace(/\+/g, " "));
          }
        });
      }
    }
  } catch (ignore) {}
  return out;
}

/** Trả về số nếu hợp lệ, ngược lại trả về "" (TUYỆT ĐỐI không bịa giá trị). */
function numOrBlank_(v) {
  if (v === undefined || v === null) return "";
  var s = String(v).trim();
  if (s === "") return "";
  var n = Number(s);
  return isNaN(n) ? "" : n;
}

function isMissing_(v) {
  return v === "" || v === null || v === undefined;
}

function truthy_(v) {
  if (v === true) return true;
  if (v === false || v === undefined || v === null) return false;
  var s = String(v).trim().toLowerCase();
  return s === "1" || s === "true" || s === "yes" || s === "on";
}

/**
 * Đảm bảo sheet tồn tại và header đầy đủ (tự bổ sung cột mới cho file cũ).
 */
function ensureSheet_(ss, name, headers, headerColor) {
  var sh = ss.getSheetByName(name);
  if (!sh) {
    sh = ss.insertSheet(name);
    sh.getRange(1, 1, 1, headers.length).setValues([headers]);
    sh.getRange(1, 1, 1, headers.length).setFontWeight("bold").setBackground(headerColor);
    sh.setFrozenRows(1);
    return sh;
  }

  var lastCol = Math.max(sh.getLastColumn(), 1);
  var existing = sh.getRange(1, 1, 1, lastCol).getValues()[0];
  var hasHeader = String(existing[0] || "").trim() !== "";

  if (!hasHeader) {
    sh.getRange(1, 1, 1, headers.length).setValues([headers]);
    sh.getRange(1, 1, 1, headers.length).setFontWeight("bold").setBackground(headerColor);
    sh.setFrozenRows(1);
  } else if (lastCol < headers.length) {
    sh.getRange(1, lastCol + 1, 1, headers.length - lastCol)
      .setValues([headers.slice(lastCol)]);
    sh.getRange(1, 1, 1, headers.length).setFontWeight("bold").setBackground(headerColor);
  }
  return sh;
}

function formatNewRow_(sh, rowIndex, cols, statusQCVN, alarm) {
  try {
    var baseFormats = (cols === PROFILE_COL_FORMATS.length) ? PROFILE_COL_FORMATS : TELE_COL_FORMATS;
    var rowFormats = [];
    for (var c = 0; c < cols; c++) {
      rowFormats.push(c < baseFormats.length ? baseFormats[c] : "@");
    }
    var rowRange = sh.getRange(rowIndex, 1, 1, cols);
    rowRange.setNumberFormats([rowFormats]);

    // Tự động tô màu dòng cảnh báo (Conditional Highlight)
    var st = String(statusQCVN || "");
    if (alarm || st.indexOf("NGUY HẠI") >= 0 || st.indexOf("LỖI CẢM BIẾN") >= 0) {
      rowRange.setBackground("#FEE2E2").setFontColor("#991B1B").setFontWeight("bold");
    } else if (st.indexOf("CẢNH BÁO") >= 0) {
      rowRange.setBackground("#FEF3C7").setFontColor("#92400E").setFontWeight("normal");
    } else {
      var bg = (rowIndex % 2 === 0) ? "#F8FAFC" : "#FFFFFF";
      rowRange.setBackground(bg).setFontColor("#0F172A").setFontWeight("normal");
    }
  } catch (ignore) {}
}

function writeTelemetryBatch_(ss, batchList) {
  if (!batchList || !batchList.length) return 0;
  var sh = ensureSheet_(ss, CONFIG.SHEET_TELE, TELE_HEADERS, "#cfe2f3");
  var startRow = sh.getLastRow() + 1;
  var cols = TELE_HEADERS.length;

  var pr = props_();
  var lastSeq = pr.getProperty("last_seq");
  var lastMs = Number(pr.getProperty("last_seq_ms") || 0);
  var winMs = Number(pr.getProperty("DEDUPE_WINDOW_SEC") || CONFIG.DEDUPE_WINDOW_SEC) * 1000;
  var seenSeqs = {};
  if (lastSeq && (Date.now() - lastMs) < winMs) {
    seenSeqs[String(lastSeq)] = true;
  }

  var rows = [];
  var backgrounds = [];
  var fontColors = [];
  var fontWeights = [];
  var numFormats = [];

  var hasBatchAlarm = false;
  var worstP = null;
  var worstStatus = "";
  var maxSeq = null;

  var now = new Date();

  for (var i = 0; i < batchList.length; i++) {
    var p = batchList[i];
    var seq = numOrBlank_(p.seq);

    // 1. Chống ghi trùng gói tin trong cùng batch hoặc với last_seq đã lưu
    if (!isMissing_(seq)) {
      var seqKey = String(seq);
      if (seenSeqs[seqKey] || (lastSeq === seqKey && (Date.now() - lastMs) < winMs)) {
        continue; // Bỏ qua gói trùng (kể cả trùng liên tiếp hoặc cách quãng trong cùng mảng batchList)
      }
      seenSeqs[seqKey] = true;
      lastSeq = seqKey;
      lastMs = Date.now();
      maxSeq = seq;
    }

    // 2. Bảo tồn timestamp thực của từng gói tin trong chuỗi gửi bù
    var itemTime = now;
    if (p.time) {
      var pt = new Date(p.time);
      if (!isNaN(pt.getTime())) itemTime = pt;
    } else if (p.timestamp) {
      var pts = new Date(Number(p.timestamp) > 1e11 ? Number(p.timestamp) : Number(p.timestamp) * 1000);
      if (!isNaN(pts.getTime())) itemTime = pts;
    } else if (batchList.length > 1) {
      // Nếu gửi mảng đệm chưa có timestamp tường minh, tính lùi thời gian theo chu kỳ 1s chuẩn
      var offsetMs = (batchList.length - 1 - i) * 1000;
      itemTime = new Date(now.getTime() - offsetMs);
    }

    var src = p.src || "ESP32_BATCH";
    var ph = numOrBlank_(p.ph);
    var doMg = numOrBlank_(p.do);
    var tds = numOrBlank_(p.tds);
    var ec = numOrBlank_(p.ec);
    // Tự động tính độ dẫn điện EC nếu thiếu từ TDS (EC ≈ 2 * TDS)
    if (isMissing_(ec) && !isMissing_(tds)) {
      ec = Math.round(tds * 2.0);
    }

    var ntu = numOrBlank_(p.ntu);
    var temp = numOrBlank_(p.temp);
    var dep = numOrBlank_(p.dep);
    var amp = numOrBlank_(p.amp);
    var bat = numOrBlank_(p.bat);
    var watt = numOrBlank_(p.watt);
    if (isMissing_(watt) && !isMissing_(bat) && !isMissing_(amp)) {
      watt = Math.round(bat * amp * 100) / 100;
    }
    var roll = numOrBlank_(p.roll);
    var pit = numOrBlank_(p.pitch);
    var hdg = numOrBlank_(p.hdg);
    var lat = numOrBlank_(p.lat);
    var lng = numOrBlank_(p.lng);
    var sats = numOrBlank_(p.sats);
    var spd = numOrBlank_(p.spd);
    var alt = numOrBlank_(p.alt);
    var homed = numOrBlank_(p.homed);
    var tgthdg = numOrBlank_(p.tgthdg);
    var flags2 = numOrBlank_(p.flags2);
    var rssi = numOrBlank_(p.rssi);

    // Chuẩn hóa Sức khỏe HW: luôn là phần trăm 0 - 100% (không ghi bitmask 2047 lố bịch)
    var hwInfo = decodeHardwareHealth_(p.hwm, p.hw);
    var hwPct = hwInfo.pct;

    var wqi = numOrBlank_(p.wqi);
    var alarm = truthy_(p.alarm);
    var fw = p.fw || "";
    var adcFault = numOrBlank_(p.adf !== undefined ? p.adf : p.adcFault);

    var statusQCVN = evaluateQCVN_(ph, tds, ntu, temp, doMg, adcFault);

    if (alarm || (statusQCVN && (statusQCVN.indexOf("NGUY HẠI") >= 0 || statusQCVN.indexOf("LỖI CẢM BIẾN") >= 0))) {
      hasBatchAlarm = true;
      if (!worstP || statusQCVN.indexOf("NGUY HẠI") >= 0 || statusQCVN.indexOf("LỖI CẢM BIẾN") >= 0) {
        worstP = p;
        worstStatus = statusQCVN;
      }
    }

    var r = [
      itemTime, seq, ph, doMg, tds, ec, ntu, temp, dep,
      watt, amp, bat, roll, pit, hdg,
      lat, lng, sats, spd, alt, homed, tgthdg, flags2,
      rssi, hwPct, wqi, alarm ? "CÓ CẢNH BÁO" : "BÌNH THƯỜNG",
      statusQCVN, src, fw
    ];
    rows.push(r);

    // Xác định màu sắc từng dòng cho mảng 2 chiều Batch
    var curRowIdx = startRow + rows.length - 1;
    var rowBg = (curRowIdx % 2 === 0) ? "#F8FAFC" : "#FFFFFF";
    var rowFg = "#0F172A";
    var rowWeight = "normal";

    if (alarm || (statusQCVN && (statusQCVN.indexOf("NGUY HẠI") >= 0 || statusQCVN.indexOf("LỖI CẢM BIẾN") >= 0))) {
      rowBg = "#FEE2E2";
      rowFg = "#991B1B";
      rowWeight = "bold";
    } else if (statusQCVN && statusQCVN.indexOf("CẢNH BÁO") >= 0) {
      rowBg = "#FEF3C7";
      rowFg = "#92400E";
      rowWeight = "normal";
    }

    var bgRow = [];
    var fgRow = [];
    var wtRow = [];
    var fmtRow = [];
    for (var c_idx = 0; c_idx < cols; c_idx++) {
      bgRow.push(rowBg);
      fgRow.push(rowFg);
      wtRow.push(rowWeight);
      fmtRow.push(c_idx < TELE_COL_FORMATS.length ? TELE_COL_FORMATS[c_idx] : "@");
    }
    backgrounds.push(bgRow);
    fontColors.push(fgRow);
    fontWeights.push(wtRow);
    numFormats.push(fmtRow);
  }

  if (rows.length > 0) {
    // 3. Ghi và định dạng toàn bộ mảng dữ liệu Batch bằng API mảng 2 chiều (Tốc độ tăng 50x!)
    var targetRange = sh.getRange(startRow, 1, rows.length, cols);
    targetRange.setValues(rows);
    targetRange.setNumberFormats(numFormats);
    targetRange.setBackgrounds(backgrounds);
    targetRange.setFontColors(fontColors);
    targetRange.setFontWeights(fontWeights);

    // Cập nhật mốc seq mới nhất để chống trùng
    if (maxSeq !== null) {
      pr.setProperty("last_seq", String(maxSeq));
      pr.setProperty("last_seq_ms", String(Date.now()));
    }

    // 4. Gửi email cảnh báo nếu có bất kỳ gói tin nào vi phạm QCVN trong Batch
    if (hasBatchAlarm && worstP) {
      try {
        sendAlarmEmail_(buildAlarmBody_(worstP, worstStatus));
      } catch (errEmail) {
        Logger.log("Batch Alarm Email Error: " + errEmail.message);
      }
    }

    // 5. Gửi thông báo Discord cho gói mới nhất
    try {
      var latestP = batchList[batchList.length - 1];
      var latestStatus = evaluateQCVN_(numOrBlank_(latestP.ph), numOrBlank_(latestP.tds), numOrBlank_(latestP.ntu), numOrBlank_(latestP.temp), numOrBlank_(latestP.do));
      var latestAlarm = truthy_(latestP.alarm);
      sendDiscordTelemetry_(latestP, latestStatus, latestAlarm, ss.getUrl());
    } catch (ignore) {}
  }

  return rows.length;
}

// ============================================================================
// GHI TELEMETRY & TỰ ĐỘNG BẮN DISCORD WEBHOOK
// ============================================================================
function writeTelemetry_(ss, p) {
  var sh = ensureSheet_(ss, CONFIG.SHEET_TELE, TELE_HEADERS, "#cfe2f3");
  var now = new Date();

  var seq = numOrBlank_(p.seq);
  var src = p.src || "ESP32";

  // Chống ghi trùng: cùng #Seq trong khoảng DEDUPE_WINDOW_SEC
  if (!isMissing_(seq)) {
    var pr = props_();
    var lastSeq = pr.getProperty("last_seq");
    var lastMs = Number(pr.getProperty("last_seq_ms") || 0);
    var winMs = Number(pr.getProperty("DEDUPE_WINDOW_SEC") || CONFIG.DEDUPE_WINDOW_SEC) * 1000;
    if (lastSeq === String(seq) && (Date.now() - lastMs) < winMs) {
      return;
    }
    pr.setProperty("last_seq", String(seq));
    pr.setProperty("last_seq_ms", String(Date.now()));
  }

  var ph = numOrBlank_(p.ph);
  var doMg = numOrBlank_(p.do);
  var tds = numOrBlank_(p.tds);
  var ec = numOrBlank_(p.ec);
  // Tự động tính độ dẫn điện EC nếu thiếu từ TDS (EC ≈ 2 * TDS)
  if (isMissing_(ec) && !isMissing_(tds)) {
    ec = Math.round(tds * 2.0);
  }
  var ntu = numOrBlank_(p.ntu);
  var temp = numOrBlank_(p.temp);
  var dep = numOrBlank_(p.dep);
  var amp = numOrBlank_(p.amp);
  var bat = numOrBlank_(p.bat);
  var watt = numOrBlank_(p.watt);
  if (isMissing_(watt) && !isMissing_(bat) && !isMissing_(amp)) {
    watt = Math.round(bat * amp * 100) / 100;
  }
  var roll = numOrBlank_(p.roll);
  var pit = numOrBlank_(p.pitch);
  var hdg = numOrBlank_(p.hdg);
  var lat = numOrBlank_(p.lat);
  var lng = numOrBlank_(p.lng);
  var sats = numOrBlank_(p.sats);
  var spd = numOrBlank_(p.spd);
  var alt = numOrBlank_(p.alt);
  var homed = numOrBlank_(p.homed);
  var tgthdg = numOrBlank_(p.tgthdg);
  var flags2 = numOrBlank_(p.flags2);
  var rssi = numOrBlank_(p.rssi);

  // Chuẩn hóa Sức khỏe HW: luôn là phần trăm 0 - 100% (không ghi bitmask 2047 lố bịch)
  var hwInfo = decodeHardwareHealth_(p.hwm, p.hw);
  var hwPct = hwInfo.pct;

  var wqi = numOrBlank_(p.wqi);
  var alarm = truthy_(p.alarm);
  var fw = p.fw || "";
  var adcFault = numOrBlank_(p.adf !== undefined ? p.adf : p.adcFault);

  var statusQCVN = evaluateQCVN_(ph, tds, ntu, temp, doMg, adcFault);

  var row = [
    now, seq, ph, doMg, tds, ec, ntu, temp, dep,
    watt, amp, bat, roll, pit, hdg,
    lat, lng, sats, spd, alt, homed, tgthdg, flags2,
    rssi, hwPct, wqi, alarm ? "CÓ CẢNH BÁO" : "BÌNH THƯỜNG",
    statusQCVN, src, fw
  ];

  sh.appendRow(row);
  formatNewRow_(sh, sh.getLastRow(), TELE_HEADERS.length, statusQCVN, alarm);

  // Gửi thông báo Discord Rich Embed (Định kỳ hoặc khẩn cấp)
  try {
    sendDiscordTelemetry_(p, statusQCVN, alarm, ss.getUrl());
  } catch (errDc) {
    Logger.log("Discord error: " + errDc.message);
  }

  if (alarm) {
    sendAlarmEmail_(buildAlarmBody_(p, statusQCVN));
  }
}

// ============================================================================
// GHI PROFILE 3 TẦNG & GỬI BÁO CÁO DISCORD
// ============================================================================
function writeProfile_(ss, p) {
  var sh = ensureSheet_(ss, CONFIG.SHEET_PROFILE, PROFILE_HEADERS, "#d9ead3");
  var now = new Date();

  var ph1 = numOrBlank_(p.ph1), tds1 = numOrBlank_(p.tds1), ntu1 = numOrBlank_(p.ntu1), t1 = numOrBlank_(p.t1), do1 = numOrBlank_(p.do1);
  var ph2 = numOrBlank_(p.ph2), tds2 = numOrBlank_(p.tds2), ntu2 = numOrBlank_(p.ntu2), t2 = numOrBlank_(p.t2), do2 = numOrBlank_(p.do2);
  var ph3 = numOrBlank_(p.ph3), tds3 = numOrBlank_(p.tds3), ntu3 = numOrBlank_(p.ntu3), t3 = numOrBlank_(p.t3), do3 = numOrBlank_(p.do3);

  var row = [
    now,
    ph1, tds1, ntu1, t1, do1,
    ph2, tds2, ntu2, t2, do2,
    ph3, tds3, ntu3, t3, do3,
    p.src || "ESP32", p.fw || ""
  ];

  var st1 = evaluateQCVN_(ph1, tds1, ntu1, t1, do1);
  var st2 = evaluateQCVN_(ph2, tds2, ntu2, t2, do2);
  var st3 = evaluateQCVN_(ph3, tds3, ntu3, t3, do3);
  var profileStatus = "TỐT (An toàn theo QCVN 08:2023)";
  if (st1.indexOf("NGUY HẠI") >= 0 || st2.indexOf("NGUY HẠI") >= 0 || st3.indexOf("NGUY HẠI") >= 0) {
    profileStatus = "NGUY HẠI (Vượt quy chuẩn QCVN 08:2023)";
  } else if (st1.indexOf("LỖI CẢM BIẾN") >= 0 || st2.indexOf("LỖI CẢM BIẾN") >= 0 || st3.indexOf("LỖI CẢM BIẾN") >= 0) {
    profileStatus = "LỖI CẢM BIẾN (Kiểm tra đầu dò / Bão hòa ADC)";
  } else if (st1.indexOf("CẢNH BÁO") >= 0 || st2.indexOf("CẢNH BÁO") >= 0 || st3.indexOf("CẢNH BÁO") >= 0) {
    profileStatus = "CẢNH BÁO NHẸ (Chỉ số chạm ngưỡng)";
  }
  var profileAlarm = (profileStatus.indexOf("NGUY HẠI") >= 0 || profileStatus.indexOf("LỖI CẢM BIẾN") >= 0);

  sh.appendRow(row);
  formatNewRow_(sh, sh.getLastRow(), PROFILE_HEADERS.length, profileStatus, profileAlarm);

  // Gửi thông báo Profile 3 tầng siêu trực quan về Discord
  try {
    sendDiscordProfile_(p, ss.getUrl());
  } catch (errDc) {
    Logger.log("Discord Profile error: " + errDc.message);
  }
}

// ============================================================================
// ĐÁNH GIÁ CHẤT LƯỢNG NƯỚC & PHÁT HIỆN BẤT THƯỜNG PHẦN CỨNG (QCVN 08:2023/BTNMT)
// ============================================================================
function evaluateQCVN_(ph, tds, ntu, temp, doMg, adcFault) {
  var bad = false, warn = false, hwAnomaly = false, hasAny = false;

  if (!isMissing_(adcFault) && Number(adcFault) > 0) {
    hwAnomaly = true;
  }

  if (!isMissing_(ph) && ph > 0.1) {
    hasAny = true;
    // Phát hiện lỗi phần cứng vật lý: pH ngoài dải điện hóa (1.0..13.5) hoặc kẹt áp bão hòa 3.76 khi TDS rất thấp (<15 ppm)
    if (ph < 1.0 || ph > 13.5 || (Math.abs(ph - 3.76) <= 0.03 && (isMissing_(tds) || tds < 15))) {
      hwAnomaly = true;
    } else if (ph < 5.5 || ph > 9.0) {
      bad = true;
    } else if (ph < 6.0 || ph > 8.5) {
      warn = true;
    }
  }
  if (!isMissing_(tds) && tds > 0) {
    hasAny = true;
    if (tds > 10000) hwAnomaly = true;
    else if (tds > 1500) bad = true;
    else if (tds > 1000) warn = true;
  }
  if (!isMissing_(ntu) && ntu > 0) {
    hasAny = true;
    if (ntu > 3000) hwAnomaly = true;
    else if (ntu > 300) bad = true;
    else if (ntu > 100) warn = true;
  }
  if (!isMissing_(temp) && temp !== 0) {
    // Phát hiện lỗi DS18B20 đứt dây (-127°C), thanh ghi khởi tạo mặc định (85.0°C) hoặc ngoài ngưỡng vật lý mặt hồ
    if (temp <= -50 || Math.abs(temp - 85.0) < 0.05 || temp < 0 || temp > 65) {
      hwAnomaly = true;
    } else {
      hasAny = true;
      if (temp > 40) bad = true;
      else if (temp > 36) warn = true;
    }
  }
  if (!isMissing_(doMg) && doMg > 0.1) {
    hasAny = true;
    if (doMg > 25.0) hwAnomaly = true;
    else if (doMg < 4.0) bad = true;
    else if (doMg < 5.0) warn = true;
  }

  if (hwAnomaly) return "LỖI CẢM BIẾN (Kiểm tra đầu dò / Bão hòa ADC)";
  if (!hasAny) return "CHƯA ĐO (Cảm biến đang khởi động)";
  if (bad) return "NGUY HẠI (Vượt quy chuẩn QCVN 08:2023)";
  if (warn) return "CẢNH BÁO NHẸ (Chỉ số chạm ngưỡng)";
  return "TỐT (An toàn theo QCVN 08:2023)";
}

// ============================================================================
// GIẢI MÃ 11 LINH KIỆN PHẦN CỨNG & CÁC CỜ BÁO ĐỘNG
// ============================================================================
// PHẢI khớp enum HardwareHealthBits trong include/usv_common.h của firmware Phao.
// Thứ tự bit thay đổi ở đây sẽ làm sai toàn bộ báo cáo "11 linh kiện".
var HW_BIT_NAMES = [
  { bit: 0,  name: "DS18B20 (nhiệt độ nước)" },
  { bit: 1,  name: "Cảm biến pH" },
  { bit: 2,  name: "Cảm biến TDS" },
  { bit: 3,  name: "Cảm biến độ đục NTU" },
  { bit: 4,  name: "ACS712 (dòng điện)" },
  { bit: 5,  name: "Mạch đo áp Pin 3S" },
  { bit: 6,  name: "MPU6050 (góc nghiêng)" },
  { bit: 7,  name: "La bàn GY-271/QMC5883L" },
  { bit: 8,  name: "LoRa Ra-02 SX1278" },
  { bit: 9,  name: "Module 4G A7680C/SIM7600" },
  { bit: 10, name: "GPS NEO-6M" }
];

/**
 * Giải mã sức khỏe phần cứng.
 * @param {*} maskVal  Bitmask thô 11 bit (tham số "hwm" do Trạm Bờ gửi).
 * @param {*} pctVal   Phần trăm tổng hợp (tham số "hw", mọi nguồn đều gửi).
 * Khi chỉ có "hw" (Phao gửi 4G trực tiếp) thì chỉ báo % — KHÔNG đoán bit.
 */
function decodeHardwareHealth_(maskVal, pctVal) {
  var hasMask = !isMissing_(maskVal) && String(maskVal).trim() !== "" && !isNaN(Number(maskVal));
  var mask = hasMask ? Number(maskVal) : 0;
  var okList = [];
  var failList = [];
  var okCount = 0;

  if (hasMask) {
    for (var i = 0; i < HW_BIT_NAMES.length; i++) {
      if ((mask & (1 << HW_BIT_NAMES[i].bit)) !== 0) {
        okList.push("`[OK]` " + HW_BIT_NAMES[i].name);
        okCount++;
      } else {
        failList.push("`[LỖI]` " + HW_BIT_NAMES[i].name);
      }
    }
  }

  var pct;
  if (hasMask) {
    pct = Math.round((okCount / HW_BIT_NAMES.length) * 100);
  } else if (isMissing_(pctVal)) {
    pct = 0;
  } else {
    pct = Math.max(0, Math.min(100, Math.round(Number(pctVal))));
  }

  return {
    hasMask: hasMask,
    okCount: okCount,
    total: HW_BIT_NAMES.length,
    pct: pct,
    okStr: okList.join(" • "),
    failStr: failList.length > 0 ? failList.join(" • ") : "Không có linh kiện lỗi"
  };
}

// PHẢI khớp enum SystemAlertFlags trong include/usv_common.h của firmware Phao.
function decodeAlerts_(alarm, alrVal) {
  var alr = Number(alrVal || 0);
  var alerts = [];
  if (alr & (1 << 0)) alerts.push("🛑 KẸT CHÂN VỊT / QUÁ DÒNG ĐỘNG CƠ ĐẨY");
  if (alr & (1 << 1)) alerts.push("⚠️ PIN YẾU (điện áp mỗi cell < 3.35V)");
  if (alr & (1 << 2)) alerts.push("⚠️ PHAO NGHIÊNG LỚN (> 45°) - Nguy cơ lật");
  if (alr & (1 << 3)) alerts.push("☣️ NƯỚC VƯỢT NGƯỠNG CẢNH BÁO QCVN 08:2023");
  if (alr & (1 << 4)) alerts.push("⚠️ LỒNG ĐO ĐÃ CHẠM ĐÁY BÙN");
  if (alr & (1 << 5)) alerts.push("🔌 CÓ CẢM BIẾN CHƯA CẮM / MẤT KẾT NỐI");
  if (alr & (1 << 6)) alerts.push("📶 ĐANG CHẠY 4G DỰ PHÒNG (mất sóng LoRa)");
  if (alr & (1 << 7)) alerts.push("🧭 AUTOPILOT GIỮ HƯỚNG ĐANG BẬT");
  if (alerts.length === 0 && alarm) alerts.push("⚠️ Cảnh báo chung từ hệ thống cảm biến");
  return alerts;
}

// ============================================================================
// GỬI DISCORD WEBHOOK TELEMETRY (RICH EMBED)
// ============================================================================
function sendDiscordTelemetry_(p, statusQCVN, alarm, sheetUrl) {
  var pr = props_();
  var url = pr.getProperty("DISCORD_WEBHOOK_URL") || CONFIG.DISCORD_WEBHOOK_URL;
  if (!url) return false;

  var nowMs = Date.now();
  var lastMs = Number(pr.getProperty("last_discord_tele_ms") || 0);
  var cooldownMs = (CONFIG.DISCORD_PERIODIC_SEC || 60) * 1000;
  var isAlarm = !!alarm;

  // Giãn cách gửi tin bình thường để tránh spam webhook Discord
  if (!isAlarm && !CONFIG.DISCORD_NOTIFY_EVERY_TELEMETRY && (nowMs - lastMs < cooldownMs)) {
    return false;
  }
  pr.setProperty("last_discord_tele_ms", String(nowMs));

  var ph   = numOrBlank_(p.ph);
  var doMg = numOrBlank_(p.do);
  var tds  = numOrBlank_(p.tds);
  var ec   = numOrBlank_(p.ec);
  var ntu  = numOrBlank_(p.ntu);
  var temp = numOrBlank_(p.temp);
  var dep  = numOrBlank_(p.dep);
  var bat  = numOrBlank_(p.bat);
  var amp  = numOrBlank_(p.amp);
  var watt = numOrBlank_(p.watt);
  var roll = numOrBlank_(p.roll);
  var pit  = numOrBlank_(p.pitch);
  var hdg  = numOrBlank_(p.hdg);
  var lat  = numOrBlank_(p.lat);
  var lng  = numOrBlank_(p.lng);
  var sats = numOrBlank_(p.sats);
  var spd  = numOrBlank_(p.spd);
  var homed = numOrBlank_(p.homed);
  var tgthdg = numOrBlank_(p.tgthdg);
  var rssi = numOrBlank_(p.rssi);
  var snr  = numOrBlank_(p.snr);
  var wqi  = numOrBlank_(p.wqi);
  var seq  = p.seq || "0";
  var src  = p.src || "ESP32";

  // Phân màu Embed Discord: Đỏ khi có cảnh báo/nguy hại, Vàng khi cảnh báo nhẹ, Xanh khi tốt
  var color = 0x2ecc71; // Xanh lá cây (An toàn / Tốt)
  if (isAlarm || statusQCVN.indexOf("NGUY HẠI") >= 0) color = 0xe74c3c; // Đỏ tươi (Báo động)
  else if (statusQCVN.indexOf("CẢNH BÁO") >= 0) color = 0xf1c40f; // Vàng đậm (Cảnh báo)

  var hwInfo = decodeHardwareHealth_(p.hwm, p.hw);
  var alertList = decodeAlerts_(alarm, p.alr);

  // Tuyệt đối không bịa toạ độ: chưa có GPS FIX thì ghi rõ, không gắn link Maps giả.
  var hasGps = (lat !== "" && !isNaN(lat) && Number(lat) !== 0) &&
               (lng !== "" && !isNaN(lng) && Number(lng) !== 0);
  var latVal = hasGps ? Number(lat).toFixed(5) : "Chưa có GPS FIX";
  var lngVal = hasGps ? Number(lng).toFixed(5) : "Chưa có GPS FIX";
  var gmapUrl = hasGps ? ("https://www.google.com/maps?q=" + latVal + "," + lngVal) : "";
  var map3dUrl = (CONFIG.GCS_WEB_URL || "http://192.168.4.1") + "/3d";
  var gcsWebUrl = CONFIG.GCS_WEB_URL || "http://192.168.4.1";

  var title = isAlarm
    ? "🚨 [CẢNH BÁO KHẨN CẤP] USV AQUA-MONITOR PHÁT HIỆN SỰ CỐ!"
    : "🌊 [USV AQUA-MONITOR] BÁO CÁO GIÁM SÁT MÔI TRƯỜNG NƯỚC & THIẾT BỊ";

  var desc = isAlarm
    ? "**Cảnh báo khẩn cấp:**\n" + alertList.join("\n")
    : "Hệ thống trạm phao USV tự hành giám sát nước đa tầng & truyền thời gian thực.";

  var fields = [
    {
      name: "🧪 CHỈ SỐ NƯỚC & ĐÁNH GIÁ QCVN",
      value:
        "• **pH**: `" + (ph !== "" ? ph : "--") + "` | **DO**: `" + (doMg !== "" ? doMg + " mg/L" : "--") + "`\n" +
        "• **TDS**: `" + (tds !== "" ? tds + " ppm" : "--") + "` | **EC**: `" + (ec !== "" ? ec + " µS/cm" : "--") + "`\n" +
        "• **Độ đục**: `" + (ntu !== "" ? ntu + " NTU" : "--") + "` | **Nhiệt độ**: `" + (temp !== "" ? temp + " °C" : "--") + "`\n" +
        "• **Chỉ số VN-WQI**: **" + (wqi !== "" ? wqi : "--") + "/100**\n" +
        "• **Đánh giá QCVN 08:2023**: **" + statusQCVN + "**",
      inline: false
    },
    {
      name: "🔋 NĂNG LƯỢNG & PIN 3S / SOLAR",
      value:
        "• **Điện áp Pin**: `" + (bat !== "" ? bat + " V" : "--") + "`\n" +
        "• **Dòng điện**: `" + (amp !== "" ? amp + " A" : "--") + "`\n" +
        "• **Công suất**: `" + (watt !== "" ? watt + " W" : "--") + "`\n" +
        "• **Độ sâu tời**: `" + (dep !== "" ? dep + " m" : "--") + "`",
      inline: true
    },
    {
      name: "🧭 ĐỊNH VỊ GPS & ĐIỀU HƯỚNG",
      value:
        "• **Tọa độ**: `" + latVal + ", " + lngVal + "`\n" +
        "• **Vệ tinh**: `" + (sats !== "" ? sats + " SV" : "--") + "` | **Tốc độ**: `" + (spd !== "" ? spd + " km/h" : "--") + "`\n" +
        "• **Cách Home**: `" + (homed !== "" ? homed + " m" : "--") + "`\n" +
        "• **La bàn**: `" + (hdg !== "" ? hdg + "°" : "--") + "` (Mục tiêu: `" + (tgthdg !== "" ? tgthdg + "°" : "--") + "`)",
      inline: true
    },
    {
      name: "📡 VIỄN THÔNG LORA & TRẠM BỜ",
      value:
        "• **LoRa RSSI**: `" + (rssi !== "" ? rssi + " dBm" : "--") + "` | **SNR**: `" + (snr !== "" ? snr + " dB" : "--") + "`\n" +
        "• **Gói tin #Seq**: `#" + seq + "` | **Nguồn gửi**: `" + src + "`\n" +
        "• **Góc nghiêng Roll/Pitch**: `" + (roll !== "" ? roll + "°" : "--") + "` / `" + (pit !== "" ? pit + "°" : "--") + "`",
      inline: false
    },
    {
      name: "🛠️ SỨC KHỎE 11 THÀNH PHẦN PHẦN CỨNG (" + hwInfo.pct + "% HOÀN HẢO)",
      value: hwInfo.hasMask
        ? (hwInfo.okStr +
           (hwInfo.failStr !== "Không có linh kiện lỗi" ? ("\n⚠️ **Cảnh báo lỗi**: " + hwInfo.failStr) : ""))
        : ("📊 Chỉ nhận được mức tổng hợp **" + hwInfo.pct + "%** từ nguồn gửi (không kèm bitmask chi tiết 11 linh kiện)."),
      inline: false
    },
    {
      name: "🗺️ BẢN ĐỒ 3D TRỰC QUAN & LIÊN KẾT ĐIỀU HÀNH (NHẤP VÀO ĐỂ MỞ)",
      value:
        "🌐 [**Xem Bản Đồ 3D WebGL Hồ Nước Trực Quan**](" + map3dUrl + ")\n" +
        (hasGps
          ? ("📍 [**Xem Vị Trí Vệ Tinh Google Maps**](" + gmapUrl + ")\n")
          : "📍 **Vị trí GPS**: Chưa có FIX hợp lệ (không hiển thị link bản đồ ảo)\n") +
        "📊 [**Mở Bảng Tính Lịch Sử Google Sheets**](" + sheetUrl + ")\n" +
        "🕹️ [**Trạm Điều Khiển GCS Web Dashboard**](" + gcsWebUrl + ")",
      inline: false
    }
  ];

  var payload = {
    username: "ChemStation AI - USV Buoy Monitor",
    avatar_url: "https://img.icons8.com/fluency/96/boat.png",
    embeds: [
      {
        title: title,
        description: desc,
        color: color,
        fields: fields,
        footer: {
          text: "ChemStation AI USV v" + CONFIG.SCRIPT_VERSION + " • Nguồn: " + src + " • #Seq: " + seq,
          icon_url: "https://img.icons8.com/fluency/48/water.png"
        },
        timestamp: new Date().toISOString()
      }
    ]
  };

  return sendDiscordWebhookPayload_(payload);
}

// ============================================================================
// GỬI DISCORD WEBHOOK PROFILE 3 TẦNG ĐỘ SÂU
// ============================================================================
function padAscii_(v, len) {
  var s = (v !== "" && v !== null && v !== undefined) ? String(v) : "--";
  while (s.length < len) s = " " + s;
  return s;
}

function sendDiscordProfile_(p, sheetUrl) {
  var pr = props_();
  var url = pr.getProperty("DISCORD_WEBHOOK_URL") || CONFIG.DISCORD_WEBHOOK_URL;
  if (!url) return false;

  var ph1 = numOrBlank_(p.ph1), tds1 = numOrBlank_(p.tds1), ntu1 = numOrBlank_(p.ntu1), t1 = numOrBlank_(p.t1), do1 = numOrBlank_(p.do1);
  var ph2 = numOrBlank_(p.ph2), tds2 = numOrBlank_(p.tds2), ntu2 = numOrBlank_(p.ntu2), t2 = numOrBlank_(p.t2), do2 = numOrBlank_(p.do2);
  var ph3 = numOrBlank_(p.ph3), tds3 = numOrBlank_(p.tds3), ntu3 = numOrBlank_(p.ntu3), t3 = numOrBlank_(p.t3), do3 = numOrBlank_(p.do3);
  var src = p.src || "ESP32";
  var map3dUrl = (CONFIG.GCS_WEB_URL || "http://192.168.4.1") + "/3d";

  var tableAscii =
    "```\n" +
    "+----------+-------+------+-----+--------+----------+----------+\n" +
    "| TẦNG ĐO  | ĐỘ SÂU|  pH  | TDS | ĐỘ ĐỤC | NHIỆT ĐỘ | DO(mg/L) |\n" +
    "+----------+-------+------+-----+--------+----------+----------+\n" +
    "| MẶT NƯỚC | 0.30m | " + padAscii_(ph1,4) + " | " + padAscii_(tds1,3) + " | " + padAscii_(ntu1,4) + " | " + padAscii_(t1,6) + "°C | " + padAscii_(do1,6) + " |\n" +
    "| GIỮA HỒ  | 1.00m | " + padAscii_(ph2,4) + " | " + padAscii_(tds2,3) + " | " + padAscii_(ntu2,4) + " | " + padAscii_(t2,6) + "°C | " + padAscii_(do2,6) + " |\n" +
    "| ĐÁY HỒ   | 1.80m | " + padAscii_(ph3,4) + " | " + padAscii_(tds3,3) + " | " + padAscii_(ntu3,4) + " | " + padAscii_(t3,6) + "°C | " + padAscii_(do3,6) + " |\n" +
    "+----------+-------+------+-----+--------+----------+----------+\n" +
    "```";

  var payload = {
    username: "ChemStation AI - USV Buoy Monitor",
    avatar_url: "https://img.icons8.com/fluency/96/deep-sea-diver.png",
    embeds: [
      {
        title: "🔬 [HỒ SƠ ĐO SÂU 3 TẦNG] HOÀN TẤT CHU KỲ QUAN TRẮC ĐỘ SÂU THÀNH CÔNG",
        description: "Hệ thống tời lồng cảm biến USV đã tự động lấy mẫu đo tầng Mặt (0.3m) -> Giữa (1.0m) -> Đáy (1.8m) và tự động thu lồng về vị trí an toàn Home.",
        color: 0x3498db, // Xanh dương đậm công nghệ
        fields: [
          {
            name: "📊 BẢNG MẶT CẮT THÔNG SỐ 3 TẦNG NƯỚC CHI TIẾT",
            value: tableAscii,
            inline: false
          },
          {
            name: "🌐 XEM BẢN ĐỒ 3D WEBGL VÀ BẢNG TÍNH LỊCH SỬ",
            value:
              "🌐 [**Mở Bản Đồ 3D WebGL Quan Trắc Nước Đa Chiều**](" + map3dUrl + ")\n" +
              "📊 [**Xem Chi Tiết Tại Google Sheets (Sheet: Profile_3Tang)**](" + sheetUrl + ")",
            inline: false
          }
        ],
        footer: {
          text: "ChemStation AI Profile 3 Tầng • Nguồn: " + src,
          icon_url: "https://img.icons8.com/fluency/48/water.png"
        },
        timestamp: new Date().toISOString()
      }
    ]
  };

  return sendDiscordWebhookPayload_(payload);
}

function sendDiscordWebhookPayload_(payload) {
  var pr = props_();
  var url = pr.getProperty("DISCORD_WEBHOOK_URL") || CONFIG.DISCORD_WEBHOOK_URL;
  if (!url || (url.indexOf("https://discord.com/api/webhooks/") !== 0 && url.indexOf("https://discordapp.com/api/webhooks/") !== 0)) {
    return false;
  }
  var options = {
    method: "post",
    contentType: "application/json",
    payload: JSON.stringify(payload),
    muteHttpExceptions: true
  };
  try {
    var resp = UrlFetchApp.fetch(url, options);
    var code = resp.getResponseCode();
    return (code >= 200 && code < 300);
  } catch (e) {
    Logger.log("Discord Webhook Fetch Error: " + e.message);
    return false;
  }
}

// ============================================================================
// HÀM TEST DISCORD WEBHOOK TRỰC TIẾP TỪ GOOGLE APPS SCRIPT
// ============================================================================
function testDiscordWebhook() {
  var mockP = {
    mode: "tele",
    seq: 9999,
    ph: 7.65,
    do: 6.82,
    tds: 312,
    ec: 624,
    ntu: 15,
    temp: 28.4,
    dep: 0.30,
    bat: 12.38,
    amp: 0.85,
    watt: 10.52,
    lat: 10.76262,
    lng: 106.66017,
    sats: 11,
    spd: 2.4,
    homed: 18.5,
    hdg: 145,
    tgthdg: 145,
    roll: 1.5,
    pitch: -0.8,
    rssi: -32,
    snr: 9.8,
    hw: 100,   // Sức khỏe phần cứng chuẩn hóa 100%
    hwm: 2047, // 11 linh kiện = 100% OK (Bitmask 0x7FF)
    wqi: 88,
    alarm: 0,
    src: "ESP32_WIFI_TEST",
    fw: "v4.3-DISCORD"
  };
  var ss = null;
  try { ss = getSpreadsheet_(); } catch (e) {}
  var sheetUrl = ss ? ss.getUrl() : "https://docs.google.com/spreadsheets/d/" + CONFIG.SPREADSHEET_ID;
  var ok = sendDiscordTelemetry_(mockP, "TỐT (An toàn theo QCVN 08:2023)", false, sheetUrl);
  Logger.log("Ket qua gui test Discord Webhook: " + (ok ? "THANH CONG" : "THAT BAI (Kiem tra lai URL Webhook)"));
  return ok;
}

// ============================================================================
// EMAIL CẢNH BÁO
// ============================================================================
function buildAlarmBody_(p, qcvn) {
  var lines = [
    "CHEMSTATION AI - CẢNH BÁO TỪ PHAO QUAN TRẮC USV",
    "Thời gian: " + new Date().toLocaleString("vi-VN"),
    "Đánh giá QCVN: " + qcvn,
    "Mã gói: #" + (p.seq || "?"),
    "",
    "pH: " + (p.ph || "--"),
    "DO: " + (p.do || "--") + " mg/L",
    "TDS: " + (p.tds || "--") + " ppm",
    "EC: " + (p.ec || "--") + " µS/cm",
    "Độ đục: " + (p.ntu || "--") + " NTU",
    "Nhiệt độ: " + (p.temp || "--") + " °C",
    "Pin: " + (p.bat || "--") + " V",
    "Vị trí: " + (p.lat || "--") + ", " + (p.lng || "--"),
    "Vệ tinh GPS: " + (p.sats || "--"),
    "",
    "Vui lòng kiểm tra hệ thống hoặc thu hồi phao."
  ];
  return lines.join("\n");
}

function sendAlarmEmail_(body) {
  var pr = props_();
  var email = pr.getProperty("ALARM_EMAIL") || CONFIG.ALARM_EMAIL;
  if (!email) return false;

  var cooldown = Number(pr.getProperty("ALARM_COOLDOWN_SEC") || CONFIG.ALARM_COOLDOWN_SEC);
  var lastMs = Number(pr.getProperty("last_alarm_ms") || 0);
  if (Date.now() - lastMs < cooldown * 1000) return false;

  try {
    MailApp.sendEmail({
      to: email,
      subject: "[ChemStation AI] CẢNH BÁO CHẤT LƯỢNG NƯỚC & PHAO USV",
      body: body
    });
    pr.setProperty("last_alarm_ms", String(Date.now()));
    return true;
  } catch (err) {
    return false;
  }
}

// ============================================================================
// CHẾ ĐỘ STATUS (JSON) - Kiểm tra cấu hình từ trình duyệt
// ============================================================================
function statusOut_(ss) {
  var pr = props_();
  var webhook = pr.getProperty("DISCORD_WEBHOOK_URL") || CONFIG.DISCORD_WEBHOOK_URL;
  var out = {
    ok: true,
    mode: "status",
    scriptVersion: CONFIG.SCRIPT_VERSION,
    time: new Date().toISOString(),
    spreadsheet: ss.getName(),
    spreadsheetUrl: ss.getUrl(),
    sheets: {},
    discordWebhookConfigured: !!(webhook && webhook.indexOf("https://discord") === 0),
    alarmEmailConfigured: !!(pr.getProperty("ALARM_EMAIL") || CONFIG.ALARM_EMAIL),
    alarmCooldownSec: Number(pr.getProperty("ALARM_COOLDOWN_SEC") || CONFIG.ALARM_COOLDOWN_SEC),
    dedupeWindowSec: Number(pr.getProperty("DEDUPE_WINDOW_SEC") || CONFIG.DEDUPE_WINDOW_SEC)
  };

  [CONFIG.SHEET_TELE, CONFIG.SHEET_PROFILE].forEach(function (name) {
    var sh = ss.getSheetByName(name);
    if (!sh) {
      out.sheets[name] = { exists: false, rows: 0 };
      return;
    }
    var rows = Math.max(sh.getLastRow() - 1, 0);
    var last = null;
    if (sh.getLastRow() >= 2) {
      var cols = Math.min(sh.getLastColumn(), 30);
      var values = sh.getRange(sh.getLastRow(), 1, 1, cols).getValues()[0];
      last = values.map(function (v) {
        return (v instanceof Date) ? v.toISOString() : v;
      });
    }
    out.sheets[name] = { exists: true, rows: rows, last: last };
  });

  return jsonOut_(out);
}


// ============================================================================
// WEB DASHBOARD HTML RENDERER
// ============================================================================
function renderDashboardHtml_(ss) {
  try {
    return HtmlService.createTemplateFromFile("Index")
      .evaluate()
      .setTitle("USV Aqua-Sentinel // Quan Trắc Thủy Văn 4G LTE")
      .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL)
      .addMetaTag('viewport', 'width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no');
  } catch (err) {
    return renderFallbackDashboard_(ss);
  }
}

function renderFallbackDashboard_(ss) {
  var lastStatus = {};
  try {
    var sh = ss.getSheetByName(CONFIG.SHEET_TELE);
    if (sh && sh.getLastRow() >= 2) {
      var row = sh.getRange(sh.getLastRow(), 1, 1, Math.min(sh.getLastColumn(), 30)).getValues()[0];
      lastStatus = {
        time: row[0], seq: row[1], ph: row[2], do: row[3], tds: row[4], ec: row[5],
        ntu: row[6], temp: row[7], dep: row[8], watt: row[9], amp: row[10], bat: row[11],
        roll: row[12], pitch: row[13], hdg: row[14], lat: row[15], lng: row[16],
        sats: row[17], spd: row[18], wqi: row[25], qcvn: row[27], src: row[28]
      };
    }
  } catch (e) {}

  var html = '<!DOCTYPE html><html lang="vi" class="dark"><head><meta charset="UTF-8">' +
    '<meta name="viewport" content="width=device-width, initial-scale=1.0">' +
    '<title>USV Aqua-Sentinel // Live 4G Cloud</title>' +
    '<script src="https://cdn.tailwindcss.com"></script>' +
    '<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />' +
    '<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>' +
    '<style>body{background:#0b0f19;color:#e2e8f0;font-family:system-ui,-apple-system,sans-serif;}' +
    '.card{background:rgba(17,24,39,0.8);backdrop-filter:blur(10px);border:1px solid rgba(6,182,212,0.3);border-radius:1rem;padding:1.25rem;}' +
    '</style></head><body class="p-4 max-w-5xl mx-auto space-y-4">' +
    '<header class="flex items-center justify-between border-b border-cyan-500/30 pb-3">' +
    '<div><h1 class="text-xl font-bold text-white tracking-wider">AQUA-SENTINEL <span class="text-cyan-400">USV-01</span></h1>' +
    '<p class="text-xs text-slate-400">4G LTE Cloud Telemetry Stream • ' + (ss ? ss.getName() : '') + '</p></div>' +
    '<div class="flex items-center gap-2"><span class="w-2.5 h-2.5 rounded-full bg-emerald-400 animate-pulse"></span>' +
    '<span id="liveBadge" class="text-xs font-mono font-bold text-emerald-400">4G CLOUD ONLINE</span></div>' +
    '</header>' +
    '<div class="grid grid-cols-2 sm:grid-cols-4 gap-3">' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Chỉ Số WQI</span><div id="wqiVal" class="text-3xl font-extrabold text-cyan-400 mt-1">' + (lastStatus.wqi || '--') + '</div><span class="text-[10px] text-cyan-300">QCVN 08:2023</span></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Độ pH</span><div id="phVal" class="text-3xl font-extrabold text-white mt-1">' + (lastStatus.ph ? Number(lastStatus.ph).toFixed(2) : '--') + '</div><span class="text-[10px] text-emerald-400">Chuẩn 6.5 - 8.5</span></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Oxy DO</span><div id="doVal" class="text-3xl font-extrabold text-sky-400 mt-1">' + (lastStatus.do ? Number(lastStatus.do).toFixed(2) : '--') + '</div><span class="text-[10px] text-slate-400">mg/L</span></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Nhiệt Độ</span><div id="tempVal" class="text-3xl font-extrabold text-rose-400 mt-1">' + (lastStatus.temp ? Number(lastStatus.temp).toFixed(1) : '--') + '</div><span class="text-[10px] text-slate-400">&deg;C</span></div>' +
    '</div>' +
    '<div class="grid grid-cols-2 sm:grid-cols-4 gap-3">' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Khoáng TDS</span><div id="tdsVal" class="text-2xl font-bold text-indigo-300 mt-1">' + (lastStatus.tds || '--') + ' <span class="text-xs">ppm</span></div></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Độ Đục</span><div id="ntuVal" class="text-2xl font-bold text-amber-300 mt-1">' + (lastStatus.ntu || '--') + ' <span class="text-xs">NTU</span></div></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Tời Độ Sâu</span><div id="depVal" class="text-2xl font-bold text-teal-300 mt-1">' + (lastStatus.dep !== undefined ? Number(lastStatus.dep).toFixed(2) : '--') + ' <span class="text-xs">m</span></div></div>' +
    '<div class="card text-center"><span class="text-xs text-slate-400 uppercase font-mono">Điện Áp Pin</span><div id="batVal" class="text-2xl font-bold text-emerald-400 mt-1">' + (lastStatus.bat ? Number(lastStatus.bat).toFixed(2) : '--') + ' <span class="text-xs">V</span></div></div>' +
    '</div>' +
    '<div class="card space-y-2">' +
    '<div class="flex items-center justify-between text-xs text-slate-400 font-mono">' +
    '<span>Vĩ độ: <strong id="latVal" class="text-cyan-300">' + (lastStatus.lat || '--') + '</strong></span>' +
    '<span>Kinh độ: <strong id="lngVal" class="text-cyan-300">' + (lastStatus.lng || '--') + '</strong></span>' +
    '<span>Mã gói: <strong id="seqVal" class="text-slate-200">#' + (lastStatus.seq || '--') + '</strong></span>' +
    '<span>Thời gian: <strong id="timeVal" class="text-slate-200">' + (lastStatus.time || '--') + '</strong></span>' +
    '</div>' +
    '<div id="map" style="height:280px; border-radius:0.75rem;"></div>' +
    '</div>' +
    '<script>' +
    'var lat = ' + (Number(lastStatus.lat) || 21.0548) + ', lng = ' + (Number(lastStatus.lng) || 105.8214) + ';' +
    'var map = L.map("map").setView([lat, lng], 16);' +
    'L.tileLayer("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}").addTo(map);' +
    'var marker = L.marker([lat, lng]).addTo(map).bindPopup("Vị trí Phao USV").openPopup();' +
    'async function pollData(){' +
    '  try{' +
    '    var res = await fetch("?mode=status&_t=" + Date.now());' +
    '    var data = await res.json();' +
    '    if(data.sheets && data.sheets.Telemetry_Realtime && data.sheets.Telemetry_Realtime.last){' +
    '      var r = data.sheets.Telemetry_Realtime.last;' +
    '      document.getElementById("timeVal").innerText = r[0] || "--";' +
    '      document.getElementById("seqVal").innerText = "#" + (r[1] || "--");' +
    '      document.getElementById("phVal").innerText = r[2] ? Number(r[2]).toFixed(2) : "--";' +
    '      document.getElementById("doVal").innerText = r[3] ? Number(r[3]).toFixed(2) : "--";' +
    '      document.getElementById("tdsVal").innerText = (r[4] || "--") + " ppm";' +
    '      document.getElementById("ntuVal").innerText = (r[6] || "--") + " NTU";' +
    '      document.getElementById("tempVal").innerText = r[7] ? Number(r[7]).toFixed(1) : "--";' +
    '      document.getElementById("depVal").innerText = (r[8] !== undefined ? Number(r[8]).toFixed(2) : "--") + " m";' +
    '      document.getElementById("batVal").innerText = (r[11] ? Number(r[11]).toFixed(2) : "--") + " V";' +
    '      document.getElementById("wqiVal").innerText = r[25] || "--";' +
    '      if(r[15] && r[16]){' +
    '        var newLat = Number(r[15]), newLng = Number(r[16]);' +
    '        document.getElementById("latVal").innerText = newLat.toFixed(5);' +
    '        document.getElementById("lngVal").innerText = newLng.toFixed(5);' +
    '        marker.setLatLng([newLat, newLng]);' +
    '      }' +
    '    }' +
    '  }catch(e){console.warn(e);}' +
    '}' +
    'setInterval(pollData, 4000);' +
    '</script>' +
    '</body></html>';

  return HtmlService.createHtmlOutput(html)
    .setTitle("USV Aqua-Sentinel // Live 4G Cloud")
    .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL)
    .addMetaTag('viewport', 'width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no');
}
