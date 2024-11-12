-- Copyright (c) 2024 Marc Balmer HB9SSB
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to
-- deal in the Software without restriction, including without limitation the
-- rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
-- sell copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
-- IN THE SOFTWARE.

-- OpenRTX RTXLink protocol (http://openrtx.org/#/rtxlink)

local function slipWrite(s)
	trx.write(string.format('\xc0%s\xc0',
	    s:gsub('\xc0', '\xdb\xdc'):gsub('\xdb', '\xdb\xdd')))
end

local function slipRead(nbytes)
	local rawData = trx.read(nbytes)
	local decodedData = ''

	repeat
		local data, n = rawData:gsub('\xdb\xdc', '\xc0')
		local data, m = data:gsub('\xdb\xdd', '\xdb')
		decodedData = decodedData .. data
		local missingBytes = n + m

		if missingBytes > 0 then
			rawData = trx.read(missingBytes)
		end
	until missingBytes == 0

	return decodedData
end

local function initialize(driver)
	print('initialize', driver.name)
	local payload = '\x01GIN'
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local resp = slipRead(19)
		print('OpenRTX device ID', resp:sub(4, -4))
	else
		print('Could not retrieve OpenRTX device ID')
	end
end

local function setFrequency(driver, request, response)
	response.frequency = request.frequency

	local freq = request.frequency

	local byte4 = freq // 16777216
	freq = freq - byte4 * 16777216

	local byte3 = freq // 65536
	freq = freq - byte3 * 65536

	local byte2 = freq // 256
	freq = freq - byte2 * 256

	local byte1 = freq

	print(byte1, byte2, byte3, byte4)
	local payload = string.format('\x01SRF%s',
	     string.char(byte1, byte2, byte3, byte4))

	slipWrite(payload .. trx.crc16(payload))

	if trx.waitForData(1000) then
		local ack = slipRead(8)
	end
end

local function getFrequency(driver, request, response)
	local payload = '\x01GRF'
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local resp = slipRead(10)

		local frequency = resp:byte(7) * 16777216 +
		    resp:byte(6) * 65536 +
		    resp:byte(5) * 256 + resp:byte(4)

		response.frequency = frequency
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end
end

local modes = {
	fm = 0x01,
	fm20 = 0x02,
	nfm = 0x03,
	dmr = 0x04,
	m17 = 0x05
}

local function setMode(driver, request, response)

	response.mode = request.mode

	local newmode = modes[request.mode]

	if newmode == nil then
		response.status = 'Failure'
		response.reason = 'Unknown mode'
		return
	end

	local payload = string.format('\x01SOM%c', newmode)
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local resp = slipRead(6)
	else
		response.status = 'Failure'
		response.reason = 'No response from trx'
	end

end

local function getMode(driver, request, response)
	local payload = '\x01GOM'
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local name = 'unknown mode'
		local resp = slipRead(6)

		local operatingMode = 'none'
		local opmode = tonumber(string.byte(resp, 4))

		for k, v in pairs(modes) do
			if v == opmode then
				operatingMode = k
				break
			end
		end
			response.mode = operatingMode
	else
		response.status = 'Failure'
		response.reason = 'No reply from trx'
	end
end

local function setPtt(driver, request, response)

	response.ptt = request.ptt
	local opstatus = 0x01
	if request.ptt == 'on' then
		opstatus = 0x02
	end

	local payload = string.format('\x01SPT%c', opstatus)
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local resp = slipRead(6)
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end
end

local function getPtt(driver, request, response)
	local payload = '\x01GPT'
	slipWrite(payload .. trx.crc16(payload))
	if trx.waitForData(1000) then
		local resp = slipRead(6)

		response.ptt = tonumber(string.byte(resp, 4)) == 0x02
		    and 'on' or 'off'
	else
		response.status = 'Failure'
		response.reason = 'No reply from trx'
	end
end

local function setCallsign(driver, request, response)
	response.callsign = request.callsign

	local payload = string.format('\x01SMC%-10s', request.callsign)
	slipWrite(payload .. trx.crc16(payload))

	if trx.waitForData(1000) then
		local resp = slipRead(6)
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end

	return callsign
end

local function getCallsign(driver, request, response)
	response.callsign = request.callsign

	local payload = '\x01GMC'
	slipWrite(payload .. trx.crc16(payload))

	if trx.waitForData(1000) then
		local resp = slipRead(16)
		response.callsign = resp:sub(4, -4)
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end

	return callsign

end

local function setDestination(driver, request, response)
	response.callsign = request.callsign
	local payload = string.format('\x01SMD%-10s', request.callsign)
	slipWrite(payload .. trx.crc16(payload))

	if trx.waitForData(1000) then
		local resp = slipRead(6)
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end
end

local function getDestination(driver, request, response)
	local payload = '\x01GMD'
	slipWrite(payload .. trx.crc16(payload))

	if trx.waitForData(1000) then
		local resp = slipRead(16)
		response.callsign = resp:sub(4, -4)
	else
		response.status = 'Failure'
		response.reason = 'No answer from trx'
	end
end

return {
	name = 'OpenRTX RTXLink protocol',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = false,
		ptt = true
	},
	validModes = {
		none = 0,
		fm = 1,
		fm20 = 2,
		nfm = 3,
		dmr = 4,
		m17 = 5
	},
	ctcssModes = {},	-- trx specific
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	startStatusUpdates = nil,
	stopStatusUpdates = nil,
	handleStatusUpdates = nil,
	setLock = nil,
	setUnlock = nil,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode,
	getPtt = getPtt,
	setPtt = setPtt,
	getCallsign = getCallsign,
	setCallsign = setCallsign,
	getDestination = getDestination,
	setDestination = setDestination

}
