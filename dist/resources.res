Texture BackgroundTexture
{
 filename = bkg.png
}

Texture MediaTexture
{
 filename = media.png
}

Sprite BackgroundSprite
{
  texture = BackgroundTexture
  rect=0,0,512,512
  hotspot=0,0
}

Sprite CrossSprite
{
  texture = MediaTexture
  rect=342, 0,41,41
  hotspot=23,18
}

Sprite CircleSprite
{
  texture = MediaTexture
  rect=384, 0,41,41
  hotspot=21,19
}

Sprite Cube_1
{
  texture = MediaTexture
  rect=0, 0,168,168
  hotspot=0,0
}
Sprite Cube_2
{
  texture = MediaTexture
  rect=171, 0,168,168
  hotspot=0,0
}
Sprite Cube_3
{
  texture = MediaTexture
  rect=0, 171,168,168
  hotspot=0,0
}
Sprite Cube_4
{
  texture = MediaTexture
  rect=171, 171,168,168
  hotspot=0,0
}

Font TheFont
{
  filename=thefont.fnt
}

Font uifont
{
  filename=thefont.fnt
}

Stream BackgroundMusic
{
  filename=PH-TheSecretOfThePainting.mp3
}

Sound Click
{
  filename=Swoosh.mp3
}

Sound Cheer
{
  filename=kids-cheer.mp3
}