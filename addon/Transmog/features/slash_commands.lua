local Transmog = _G.Transmog

local MIN_WINDOW_SCALE = 0.5
local MAX_WINDOW_SCALE = 2.0

-- Returns the saved window scale, limited to the allowed range.
local function GetWindowScale()
    local scale = tonumber(transmogSettings and transmogSettings.scale) or 1
    return math.max(MIN_WINDOW_SCALE, math.min(MAX_WINDOW_SCALE, scale))
end

-- Applies the saved window scale. Saved variables load after the addon files
-- run, so this is called when the window opens rather than at load time.
function Transmog:ApplyWindowScale()
    TransmogFrame:SetScale(GetWindowScale())
end

-- Reports or changes the window scale, keeping it within the allowed range.
local function SetWindowScale(arg)
    local usage = string.format("Usage: /transmog scale <%.1f-%.1f>", MIN_WINDOW_SCALE, MAX_WINDOW_SCALE)
    if arg == "" then
        twfprint(string.format("Transmog window scale is %.2f. %s", GetWindowScale(), usage))
        return
    end

    local scale = tonumber(arg)
    -- NaN is the only value not equal to itself.
    if not scale or scale ~= scale then
        twfprint(usage)
        return
    end

    local clamped = math.max(MIN_WINDOW_SCALE, math.min(MAX_WINDOW_SCALE, scale))
    if clamped ~= scale then
        twfprint(string.format("Scale must be between %.1f and %.1f.", MIN_WINDOW_SCALE, MAX_WINDOW_SCALE))
    end

    transmogSettings = transmogSettings or {}
    transmogSettings.scale = clamped
    Transmog:ApplyWindowScale()
    twfprint(string.format("Transmog window scale set to %.2f.", clamped))
end

-- "/transmog scale <n>" resizes the window; anything else shows the
-- new-appearance alert anchor window.
SLASH_TRANSMOG1 = "/transmog"
SlashCmdList["TRANSMOG"] = function(cmd)
    local command = string.lower(cmd or "")
    local scaleArg = string.match(command, "^%s*scale%s+(.-)%s*$") or string.match(command, "^%s*scale()%s*$") and ""
    if scaleArg then
        SetWindowScale(scaleArg)
        return
    end

    Transmog.newTransmogAlert:ShowAnchor()
end

-- Toggles debug mode on/off.
SLASH_TRANSMOGDEBUG1 = "/transmogdebug"
SlashCmdList["TRANSMOGDEBUG"] = function(cmd)
    if cmd then
        if Transmog.debug then
            Transmog.debug = false
            twfprint("Transmog debug off")
        else
            Transmog.debug = true
            twfprint("Transmog debug on")
        end
    end
end

-- Registers TransmogFrame for ESC key handling (we hide GossipFrame and
-- replace it with our own frame, so Blizzard's default ESC logic needs this).
if not UISpecialFrames then
    UISpecialFrames = {}
end
tinsert(UISpecialFrames, "TransmogFrame")
