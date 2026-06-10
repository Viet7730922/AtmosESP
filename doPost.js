function doPost(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  try {
    // Phân tích dữ liệu JSON từ ESP32 gửi lên
    var data = JSON.parse(e.postData.contents);
    var timestamp = new Date(); // Lấy thời gian hiện tại
    
    // Thêm 1 dòng mới vào file Sheets
    sheet.appendRow([
      timestamp,
      data.temp,
      data.hum,
      data.pres,
      data.lux,
      data.rain,
      data.wind,
      data.gas,
      data.dew,
      data.relay
    ]);
    
    return ContentService.createTextOutput("Thành công");
  } catch (error) {
    return ContentService.createTextOutput("Lỗi: " + error.toString());
  }
}
