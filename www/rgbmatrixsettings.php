<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="global" class="settings">
	<fieldset>
	<legend>Advanced LED Panel Settings</legend>
	<table table width = "100%">
		<tr><td valign='top' colspan='2'><font color='#ff0000'><b>WARNING</b></font>
				- Only change these setting if you are having issues with your
				P10 panels. Otherwise, leave settings at default values.</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingSelect("Slowdown GPIO", "slowdownGPIO", 1, 0, "1", Array('0' => '0', '1' => '1', '2' => '2', '3' => '3', '4' => '4')); ?></td>
			<td valign='top'><b>Slowdown GPIO</b> - Zero for this parameter
				means 'no slowdown'. The Raspberry Pi 2 and 3 are putting out data
				too fast for almost all LED panels. In this case, you want to
				slow down writing to GPIO. A value of 1 typically works fine,
				but the Pi 3 may need to be slowed down even more by setting it to 2.
				If you have a Raspberry Pi with a slower processor (Model A, A+, B+, Zero),
				then a value of 0 might work better. (Default: 1)</td>
		</tr>
		<tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingText("pwmLSB", 1, 0, 4, 4, "", "130"); ?><br>
				<? PrintSettingSave("PWM Nanoseconds for LSB", "pwmLSB", 1, 0); ?></td>
			<td valign='top'><b>PWM Nanoseconds for LSB</b> -  This changes
				the base time-unit for the on-time in the lowest significant
				bit in nanoseconds. Some panels have trouble with sharp contrasts
				and short pulses that results in ghosting. It is particularly
				apparent in situations such as bright text on black background.
				In these cases increase this value until you don't see this ghosting
				anymore. Lower values will allow higher frame-rate, but will also
				negatively impact qualty in some panels (less accurate color or
				more ghosting). (Valid values are 50 to 3000. Default: 130)</td>
		</tr>
        <tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingSelect("PWM bits", "pwmBits", 1, 0, "8", Array('1' => '1', '2' => '2', '3' => '3', '4' => '4', '5' => '5', '6' => '6', '7' => '7', '8' => '8', '9' => '9', '10' => '10', '11' => '11')); ?></td>
			<td valign='top'><b>PWM bits</b> - Changes how many bits are used for PWM.
				The LEDs can only be switched on or off, so the shaded brightness perception
				is achieved via PWM (Pulse Width Modulation). Our eyes are actually perceiving
				brightness logarithmically, so we need a lot more physical resolution (11 bits)
				in order to get a good 8 Bit per color resolution (24Bit RGB). Lowering this
				value means the lower bits (=more subtle color nuances) are omitted.
				Typically you might be mostly interested in the extremes:
				1 Bit for situations that only require 8 colors like a high contrast
				text only display or 11 Bits for showing images or videos. Lower number of
				bits use slightly less CPU and result in a higher refresh rate. (Default: 8)</td>
		</tr>
        <tr><td colspan='2'><hr></td></tr>
		<tr><td valign='top'><? PrintSettingSelect("Scan Mode", "scanMode", 1, 0, "0", Array('0' => '0', '1' => '1')); ?></td>
			<td valign='top'><b>Scan Mode</b> - This switches between progressive scan
				and interlaced scan. Interlaced might look a little nicer when you have
				a very low refresh rate. 0 = progressive; 1 = interlaced (Default: 0)</td>
		</tr>
	</table>
	</fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
