#include <cassert>

#include "SDL.h"

#include "ChooseClassCreationPanel.h"

#include "Button.h"
#include "ChooseClassPanel.h"
#include "MainMenuPanel.h"
#include "TextAlignment.h"
#include "TextBox.h"
#include "../Game/GameState.h"
#include "../Math/Int2.h"
#include "../Media/Color.h"
#include "../Media/FontManager.h"
#include "../Media/FontName.h"
#include "../Media/MusicName.h"
#include "../Media/PaletteFile.h"
#include "../Media/PaletteName.h"
#include "../Media/TextureFile.h"
#include "../Media/TextureManager.h"
#include "../Media/TextureName.h"
#include "../Rendering/Renderer.h"

ChooseClassCreationPanel::ChooseClassCreationPanel(GameState *gameState)
	: Panel(gameState)
{
	this->parchment = [gameState]()
	{
		auto *surface = gameState->getTextureManager().getSurface(
			TextureFile::fromName(TextureName::ParchmentPopup)).getSurface();
		return std::unique_ptr<Surface>(new Surface(surface));
	}();

	this->titleTextBox = [gameState]()
	{
		Int2 center(Renderer::ORIGINAL_WIDTH / 2, 80);
		Color color(48, 12, 12);
		std::string text = std::string("How do you wish\nto select your class?");
		auto &font = gameState->getFontManager().getFont(FontName::A);
		auto alignment = TextAlignment::Center;
		return std::unique_ptr<TextBox>(new TextBox(
			center,
			color,
			text,
			font,
			alignment,
			gameState->getRenderer()));
	}();

	this->generateTextBox = [gameState]()
	{
		Int2 center(Renderer::ORIGINAL_WIDTH / 2, 120);
		Color color(48, 12, 12);
		std::string text = std::string("Generate\n(not implemented)");
		auto &font = gameState->getFontManager().getFont(FontName::A);
		auto alignment = TextAlignment::Center;
		return std::unique_ptr<TextBox>(new TextBox(
			center,
			color,
			text,
			font,
			alignment,
			gameState->getRenderer()));
	}();

	this->selectTextBox = [gameState]()
	{
		Int2 center(Renderer::ORIGINAL_WIDTH / 2, 160);
		Color color(48, 12, 12);
		std::string text = std::string("Select");
		auto &font = gameState->getFontManager().getFont(FontName::A);
		auto alignment = TextAlignment::Center;
		return std::unique_ptr<TextBox>(new TextBox(
			center,
			color,
			text,
			font,
			alignment,
			gameState->getRenderer()));
	}();

	this->backToMainMenuButton = []()
	{
		auto function = [](GameState *gameState)
		{
			std::unique_ptr<Panel> mainMenuPanel(new MainMenuPanel(gameState));
			gameState->setPanel(std::move(mainMenuPanel));
			gameState->setMusic(MusicName::PercIntro);
		};
		return std::unique_ptr<Button>(new Button(function));
	}();

	this->generateButton = []()
	{
		Int2 center(Renderer::ORIGINAL_WIDTH / 2, 120);
		auto function = [](GameState *gameState)
		{
			// Eventually go to a "ChooseQuestionsPanel". What about the "pop-up" message?
			/*std::unique_ptr<Panel> classPanel(new ChooseClassPanel(gameState));
			gameState->setPanel(std::move(classPanel));*/
		};
		return std::unique_ptr<Button>(new Button(center, 175, 35, function));
	}();

	this->selectButton = []()
	{
		Int2 center(Renderer::ORIGINAL_WIDTH / 2, 160);
		auto function = [](GameState *gameState)
		{
			std::unique_ptr<Panel> classPanel(new ChooseClassPanel(gameState));
			gameState->setPanel(std::move(classPanel));
		};
		return std::unique_ptr<Button>(new Button(center, 175, 35, function));
	}();
}

ChooseClassCreationPanel::~ChooseClassCreationPanel()
{

}

void ChooseClassCreationPanel::handleEvents(bool &running)
{
	auto mousePosition = this->getMousePosition();
	auto mouseOriginalPoint = this->getGameState()->getRenderer()
		.nativePointToOriginal(mousePosition);

	SDL_Event e;
	while (SDL_PollEvent(&e) != 0)
	{
		bool applicationExit = (e.type == SDL_QUIT);
		bool resized = (e.type == SDL_WINDOWEVENT) &&
			(e.window.event == SDL_WINDOWEVENT_RESIZED);
		bool escapePressed = (e.type == SDL_KEYDOWN) &&
			(e.key.keysym.sym == SDLK_ESCAPE);

		if (applicationExit)
		{
			running = false;
		}
		if (resized)
		{
			int width = e.window.data1;
			int height = e.window.data2;
			this->getGameState()->resizeWindow(width, height);
		}
		if (escapePressed)
		{
			this->backToMainMenuButton->click(this->getGameState());
		}

		bool leftClick = (e.type == SDL_MOUSEBUTTONDOWN) &&
			(e.button.button == SDL_BUTTON_LEFT);

		bool generateClicked = leftClick &&
			this->generateButton->containsPoint(mouseOriginalPoint);
		bool selectClicked = leftClick &&
			this->selectButton->containsPoint(mouseOriginalPoint);

		if (generateClicked)
		{
			this->generateButton->click(this->getGameState());
		}
		else if (selectClicked)
		{
			this->selectButton->click(this->getGameState());
		}
	}
}

void ChooseClassCreationPanel::handleMouse(double dt)
{
	static_cast<void>(dt);
}

void ChooseClassCreationPanel::handleKeyboard(double dt)
{
	static_cast<void>(dt);
}

void ChooseClassCreationPanel::tick(double dt, bool &running)
{
	static_cast<void>(dt);

	this->handleEvents(running);
}

void ChooseClassCreationPanel::render(Renderer &renderer)
{
	// Clear full screen.
	renderer.clearNative();

	// Set palette.
	auto &textureManager = this->getGameState()->getTextureManager();
	textureManager.setPalette(PaletteFile::fromName(PaletteName::Default));

	// Draw background.
	auto *background = textureManager.getTexture(
		TextureFile::fromName(TextureName::CharacterCreation), 
		PaletteFile::fromName(PaletteName::BuiltIn));
	renderer.drawToOriginal(background);

	// Draw parchments: title, generate, select.
	this->parchment->setTransparentColor(Color::Magenta);

	int parchmentX = (Renderer::ORIGINAL_WIDTH / 2) - (this->parchment->getWidth() / 2);
	int parchmentY = (Renderer::ORIGINAL_HEIGHT / 2) - (this->parchment->getHeight() / 2) - 20;

	renderer.drawToOriginal(this->parchment->getSurface(), parchmentX, parchmentY);
	renderer.drawToOriginal(this->parchment->getSurface(), parchmentX, parchmentY + 40);
	renderer.drawToOriginal(this->parchment->getSurface(), parchmentX, parchmentY + 80);

	// Draw text: title, generate, select.
	renderer.drawToOriginal(this->titleTextBox->getTexture(),
		this->titleTextBox->getX(), this->titleTextBox->getY());
	renderer.drawToOriginal(this->generateTextBox->getTexture(),
		this->generateTextBox->getX(), this->generateTextBox->getY());
	renderer.drawToOriginal(this->selectTextBox->getTexture(),
		this->selectTextBox->getX(), this->selectTextBox->getY());

	// Scale the original frame buffer onto the native one.
	renderer.drawOriginalToNative();

	// Draw cursor.
	const auto &cursor = textureManager.getSurface(
		TextureFile::fromName(TextureName::SwordCursor));
	SDL_SetColorKey(cursor.getSurface(), SDL_TRUE,
		renderer.getFormattedARGB(Color::Black));
	auto mousePosition = this->getMousePosition();
	renderer.drawToNative(cursor.getSurface(),
		mousePosition.getX(), mousePosition.getY(),
		static_cast<int>(cursor.getWidth() * this->getCursorScale()),
		static_cast<int>(cursor.getHeight() * this->getCursorScale()));
}
