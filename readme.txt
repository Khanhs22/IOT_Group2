------ Node IoT group 2 ------- Nguyen Dinh Khanh
* Chương trình nạp trên kit ESP32 Devkit V1.
* Hướng dẫn nạp chương trình:
- Cài đặt và mở phần mềm ESP-IDF.
- Truy cập vào đường dẫn chứa file của từng project.
- Để build code, nhập lệnh "idf.py build".
- Để nạp code, nhập lệnh "idf.py -p (COM) flash".
- Để nạp code và theo dõi chương trình, nhập lệnh "idf.py -p (COM) flash monitor".
- Để tắt thông tin chương trình qua terminal, ấn "Ctrl + ]".

* Hướng dẫn kết nối wifi của từng node (smart config):
- Tải phần mềm Esptouch trên điện thoại.
- Kết nối điện thoại với wifi đích cần kết nối cho esp32 (2.4Ghz).
- Mở ứng dụng Esptouch và nhập mật khẩu wifi.
- Đợi đến khi đèn xanh trên kit sáng => wifi đã được kết nối.
- Tên và mật khẩu wifi đã được lưu trên flash nên khi mất điện hoặc reset sẽ tự động kết nối lại và không bị mất dữ liệu.
- Để thay đổi wifi, ta ấn nút boot (GPIO0) trên kit, đèn xanh sẽ bị mất đi và kết nối lại wifi theo cách trên.

* Chức năng từng node (các node đều được sub và pub đến các topic MQTT tương ứng):
- Node đèn:
	+ Có thể bật tắt led từ nút bấm và qua web server.
	+ Nút bấm và web server được đồng bộ nên sẽ không bị hiển thị sai trạng thái trên server.
- Node báo cháy:
	+ Kiểm tra nhiệt độ trong phòng bếp.
	+ Khi nhiệt độ cao hơn 40*C quá 3 lần, còi báo cháy sẽ kêu lên và gửi dữ liệu đến gateway qua giao thức MQTT.
	+ Khi còi báo cháy kêu, ta có thể bấm nút nhấn trên thiết bị để tắt còi.
- Node DHT11:
	+ Đo nhiệt độ và độ ẩm của phòng khách và hiển thị lên lcd.
	+ Nhiệt độ và độ ẩm được gửi và đồng bộ với web server thông qua giao thức MQTT.